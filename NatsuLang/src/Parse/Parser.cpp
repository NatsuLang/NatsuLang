#include "Parse/Parser.h"
#include "Sema/Sema.h"
#include "Sema/Scope.h"
#include "Sema/Declarator.h"
#include "Sema/CompilerAction.h"
#include "AST/Type.h"
#include "AST/ASTContext.h"
#include "AST/ASTConsumer.h"
#include "AST/Expression.h"
#include "AST/Declaration.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Syntax;
using namespace NatsuLang::Lex;
using namespace NatsuLang::Diag;

void ResolveContext::StartResolvingDeclarator(Declaration::DeclaratorPtr decl)
{
	m_ResolvingDeclarators.emplace(std::move(decl));
}

void ResolveContext::EndResolvingDeclarator(Declaration::DeclaratorPtr const& decl)
{
	m_ResolvedDeclarators.emplace(decl);
	m_ResolvingDeclarators.erase(decl);
}

ResolveContext::ResolvingState ResolveContext::GetDeclaratorResolvingState(Declaration::DeclaratorPtr const& decl) const noexcept
{
	if (m_ResolvingDeclarators.find(decl) != m_ResolvingDeclarators.cend())
	{
		return ResolvingState::Resolving;
	}

	if (m_ResolvedDeclarators.find(decl) != m_ResolvedDeclarators.cend())
	{
		return ResolvingState::Resolved;
	}

	return ResolvingState::Unknown;
}

Parser::Parser(Preprocessor& preprocessor, Semantic::Sema& sema)
	: m_Preprocessor{ preprocessor }, m_Diag{ preprocessor.GetDiag() }, m_Sema{ sema }, m_ParenCount{}, m_BracketCount{}, m_BraceCount{}
{
	ConsumeToken();
}

Parser::~Parser()
{
}

NatsuLang::Preprocessor& Parser::GetPreprocessor() const noexcept
{
	return m_Preprocessor;
}

DiagnosticsEngine& Parser::GetDiagnosticsEngine() const noexcept
{
	return m_Diag;
}

Semantic::Sema& Parser::GetSema() const noexcept
{
	return m_Sema;
}

#if PARSER_USE_EXCEPTION
Expression::ExprPtr Parser::ParseExprError()
{
	nat_Throw(ParserException, "An error occured while parsing expression.");
}

Statement::StmtPtr Parser::ParseStmtError()
{
	nat_Throw(ParserException, "An error occured while parsing statement.");
}

Declaration::DeclPtr Parser::ParseDeclError()
{
	nat_Throw(ParserException, "An error occured while parsing declaration.");
}
#else
Expression::ExprPtr Parser::ParseExprError() noexcept
{
	return nullptr;
}

Statement::StmtPtr Parser::ParseStmtError() noexcept
{
	return nullptr;
}

Declaration::DeclPtr Parser::ParseDeclError() noexcept
{
	return nullptr;
}
#endif

void Parser::DivertPhase(std::vector<Declaration::DeclPtr>& decls)
{
	m_Sema.SetCurrentPhase(Semantic::Sema::Phase::Phase2);
	
	m_ResolveContext = make_ref<ResolveContext>(*this);
	const auto scope = make_scope([this]
	{
		m_ResolveContext.Reset();
	});

	for (auto&& skippedTopLevelCompilerAction : m_SkippedTopLevelCompilerActions)
	{
		pushCachedTokens(std::move(skippedTopLevelCompilerAction));
		const auto compilerActionScope = make_scope([this]
		{
			popCachedTokens();
		});

		ParseCompilerAction([&decls](natRefPointer<ASTNode> const& astNode)
		{
			if (auto decl = static_cast<Declaration::DeclPtr>(astNode))
			{
				decls.emplace_back(std::move(decl));
				return false;
			}

			// TODO: 报告错误：编译器动作插入了声明以外的 AST
			return true;
		});
	}

	m_SkippedTopLevelCompilerActions.clear();

	for (auto declPtr : m_Sema.GetCachedDeclarators())
	{
		if (m_ResolveContext->GetDeclaratorResolvingState(declPtr) == ResolveContext::ResolvingState::Unknown)
		{
			const auto oldUnresolvedDeclaration = declPtr->GetDecl();
			ResolveDeclarator(declPtr);
			m_Sema.HandleDeclarator(m_Sema.GetCurrentScope(), std::move(declPtr), oldUnresolvedDeclaration);
		}
	}

	for (const auto& declPtr : m_ResolveContext->GetResolvedDeclarators())
	{
		decls.emplace_back(declPtr->GetDecl());
	}

	m_Sema.ActOnPhaseDiverted();
}

nBool Parser::ParseTopLevelDecl(std::vector<Declaration::DeclPtr>& decls)
{
	switch (m_CurrentToken.GetType())
	{
	case TokenType::Kw_import:
		decls = ParseModuleImport();
		return false;
	case TokenType::Kw_module:
		decls = ParseModuleDecl();
		return false;
	case TokenType::Eof:
		return true;
	case TokenType::Dollar:
	{
		std::vector<Token> cachedTokens;
		skipCompilerAction(&cachedTokens);
		m_SkippedTopLevelCompilerActions.emplace_back(std::move(cachedTokens));
		return false;
	}
	default:
		break;
	}

	decls = ParseExternalDeclaration();
	return false;
}

std::vector<NatsuLang::Declaration::DeclPtr> Parser::ParseExternalDeclaration()
{
	switch (m_CurrentToken.GetType())
	{
	case TokenType::Semi:
		// Empty Declaration
		ConsumeToken();
		return {};
	case TokenType::RightBrace:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExtraneousClosingBrace, m_CurrentToken.GetLocation());
		ConsumeBrace();
		return {};
	case TokenType::Eof:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpectEOF, m_CurrentToken.GetLocation());
		return {};
	case TokenType::Kw_def:
	{
		SourceLocation declEnd;
		return ParseDeclaration(Declaration::Context::Global, declEnd);
	}
	case TokenType::Kw_class:
		// TODO
		nat_Throw(NotImplementedException);
	default:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
			.AddArgument(m_CurrentToken.GetType());

		// 吃掉 1 个 Token 以保证不会死循环
		ConsumeToken();
		return {};
	}
}

void Parser::ParseCompilerAction(std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	assert(m_CurrentToken.Is(TokenType::Dollar));
	ConsumeToken();

	const auto action = ParseCompilerActionName();
	
	if (!action)
	{
		return;
	}

	if (!m_CurrentToken.Is(TokenType::LeftParen))
	{
		// TODO: 报告错误
	}

	action->StartAction(CompilerActionContext{ *this });
	const auto scope = make_scope([action, &output]
	{
		action->EndAction(output);
	});

	ParseCompilerActionArgumentList(action);
}

natRefPointer<ICompilerAction> Parser::ParseCompilerActionName()
{
	auto actionNamespace = m_Sema.GetTopLevelActionNamespace();

	while (m_CurrentToken.Is(TokenType::Identifier))
	{
		const auto id = m_CurrentToken.GetIdentifierInfo();
		ConsumeToken();
		if (m_CurrentToken.Is(TokenType::Period))
		{
			actionNamespace = actionNamespace->GetSubNamespace(id->GetName());
			if (!actionNamespace)
			{
				// TODO: 报告错误
				return nullptr;
			}
			ConsumeToken();
		}
		else if (m_CurrentToken.Is(TokenType::LeftParen))
		{
			return actionNamespace->GetAction(id->GetName());
		}
		else
		{
			break;
		}
	}

	m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
		.AddArgument(m_CurrentToken.GetType());
	return nullptr;
}

void Parser::ParseCompilerActionArgumentList(natRefPointer<ICompilerAction> const& action)
{
	assert(m_CurrentToken.Is(TokenType::LeftParen));
	ConsumeParen();

	const auto requirement = action->GetArgumentRequirement();

	// 禁止匹配过程中的错误报告
	m_Diag.EnableDiag(false);
	const auto scope = make_scope([this]
	{
		m_Diag.EnableDiag(true);
	});

	std::size_t i = 0;
	for (;; ++i)
	{
		const auto argType = requirement->GetExpectedArgumentType(i);

		if (m_CurrentToken.Is(TokenType::RightParen))
		{
			if (argType == CompilerActionArgumentType::None || (argType & CompilerActionArgumentType::Optional) != CompilerActionArgumentType::None)
			{
				ConsumeParen();
				break;
			}

			// TODO: 报告错误：参数过少
			break;
		}

		if (m_CurrentToken.Is(TokenType::Comma))
		{
			if ((argType & CompilerActionArgumentType::Optional) != CompilerActionArgumentType::None)
			{
				action->AddArgument(nullptr);
				ConsumeToken();
				continue;
			}

			// TODO: 报告错误：未为非可选的参数提供值，假设这个逗号是多余的
			ConsumeToken();
		}

		if (argType == CompilerActionArgumentType::None)
		{
			// TODO: 报告错误：参数过多
			break;
		}

		// 记录状态以便匹配失败时还原
		const auto memento = m_Preprocessor.GetLexer()->SaveToMemento();

		if ((argType & CompilerActionArgumentType::Type) != CompilerActionArgumentType::None)
		{
			const auto typeDecl = make_ref<Declaration::Declarator>(Declaration::Context::TypeName);
			ParseType(typeDecl);
			const auto type = typeDecl->GetType();
			if (type)
			{
				action->AddArgument(type);
				if (m_CurrentToken.Is(TokenType::Comma))
				{
					ConsumeToken();
				}
				continue;
			}
		}

		// 匹配类型失败了，还原 Lexer 状态
		m_Preprocessor.GetLexer()->RestoreFromMemento(memento);

		if ((argType & CompilerActionArgumentType::Declaration) != CompilerActionArgumentType::None && m_CurrentToken.Is(TokenType::Kw_def))
		{
			SourceLocation end;
			// TODO: 修改 Context
			const auto decl = ParseDeclaration(Declaration::Context::Global, end);
			if (!decl.empty())
			{
				assert(decl.size() == 1);
				action->AddArgument(decl.front());
				if (m_CurrentToken.Is(TokenType::Comma))
				{
					ConsumeToken();
				}
				continue;
			}
		}

		// 匹配声明失败了，还原 Lexer 状态
		m_Preprocessor.GetLexer()->RestoreFromMemento(memento);

		assert((argType & CompilerActionArgumentType::Statement) != CompilerActionArgumentType::None && "argType has only set flag Optional");
		const auto stmt = ParseStatement();
		if (stmt)
		{
			action->AddArgument(stmt);
			if (m_CurrentToken.Is(TokenType::Comma))
			{
				ConsumeToken();
			}
			continue;
		}

		// 匹配全部失败，报告错误
		m_Diag.EnableDiag(true);
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
			.AddArgument(m_CurrentToken.GetType());
		break;
	}
}

// class-specifier:
//	'class' [access-specifier] identifier '{' [member-specification] '}'
void Parser::ParseClassSpecifier()
{
	assert(m_CurrentToken.Is(TokenType::Kw_class));

	const auto classKeywordLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	auto accessSpecifier = Specifier::Access::None;
	switch (m_CurrentToken.GetType())
	{
	case TokenType::Kw_public:
		accessSpecifier = Specifier::Access::Public;
		ConsumeToken();
		break;
	case TokenType::Kw_protected:
		accessSpecifier = Specifier::Access::Protected;
		ConsumeToken();
		break;
	case TokenType::Kw_internal:
		accessSpecifier = Specifier::Access::Internal;
		ConsumeToken();
		break;
	case TokenType::Kw_private:
		accessSpecifier = Specifier::Access::Private;
		ConsumeToken();
		break;
	default:
		break;
	}

	if (!m_CurrentToken.Is(TokenType::Identifier))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot)
			.AddArgument(TokenType::Identifier)
			.AddArgument(m_CurrentToken.GetType());
		return;
	}

	const auto classId = m_CurrentToken.GetIdentifierInfo();
	const auto classIdLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	if (!m_CurrentToken.Is(TokenType::LeftBrace))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot)
			.AddArgument(TokenType::LeftBrace)
			.AddArgument(m_CurrentToken.GetType());
		return;
	}

	ConsumeBrace();

	ParseMemberSpecification();

	if (!m_CurrentToken.Is(TokenType::RightBrace))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot)
			.AddArgument(TokenType::RightBrace)
			.AddArgument(m_CurrentToken.GetType());
		return;
	}

	ConsumeBrace();
}

// member-specification:
//	declaration
void Parser::ParseMemberSpecification()
{
	// TODO
	nat_Throw(NotImplementedException);
}

std::vector<NatsuLang::Declaration::DeclPtr> Parser::ParseModuleImport()
{
	assert(m_CurrentToken.Is(Lex::TokenType::Kw_import));
	auto startLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	std::vector<std::pair<natRefPointer<Identifier::IdentifierInfo>, SourceLocation>> path;
	if (!ParseModuleName(path))
	{
		return {};
	}

	nat_Throw(NotImplementedException);
}

std::vector<NatsuLang::Declaration::DeclPtr> Parser::ParseModuleDecl()
{
	nat_Throw(NotImplementedException);
}

nBool Parser::ParseModuleName(std::vector<std::pair<natRefPointer<Identifier::IdentifierInfo>, SourceLocation>>& path)
{
	while (true)
	{
		if (!m_CurrentToken.Is(TokenType::Identifier))
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
			// SkipUntil({ TokenType::Semi });
			return false;
		}

		path.emplace_back(m_CurrentToken.GetIdentifierInfo(), m_CurrentToken.GetLocation());
		ConsumeToken();

		if (!m_CurrentToken.Is(TokenType::Period))
		{
			return true;
		}

		ConsumeToken();
	}
}

// declaration:
//	simple-declaration
// simple-declaration:
//	def [specifier] declarator [;]
std::vector<NatsuLang::Declaration::DeclPtr> Parser::ParseDeclaration(Declaration::Context context, NatsuLang::SourceLocation& declEnd)
{
	assert(m_CurrentToken.Is(TokenType::Kw_def));
	// 吃掉 def
	ConsumeToken();

	const auto decl = make_ref<Declaration::Declarator>(context);
	// 这不意味着 specifier 是 declarator 的一部分，至少目前如此
	ParseSpecifier(decl);
	ParseDeclarator(decl);
	if (m_CurrentToken.Is(TokenType::Semi))
	{
		ConsumeToken();
	}

	auto declaration = m_Sema.HandleDeclarator(m_Sema.GetCurrentScope(), decl);

	if (decl->IsUnresolved())
	{
		return {};
	}

	return { std::move(declaration) };
}

NatsuLang::Declaration::DeclPtr Parser::ParseFunctionBody(Declaration::DeclPtr decl, ParseScope& scope)
{
	assert(m_CurrentToken.Is(TokenType::LeftBrace));

	const auto loc = m_CurrentToken.GetLocation();

	auto body{ ParseCompoundStatement() };
	if (!body)
	{
		return ParseDeclError();
	}

	scope.ExplicitExit();
	return m_Sema.ActOnFinishFunctionBody(std::move(decl), std::move(body));
}

NatsuLang::Statement::StmtPtr Parser::ParseStatement(Declaration::Context context)
{
	Statement::StmtPtr result;
	const auto tokenType = m_CurrentToken.GetType();

	// TODO: 完成根据 tokenType 判断语句类型的过程
	switch (tokenType)
	{
	case TokenType::At:
	{
		ConsumeToken();
		if (m_CurrentToken.Is(TokenType::Identifier))
		{
			auto id = m_CurrentToken.GetIdentifierInfo();
			const auto loc = m_CurrentToken.GetLocation();

			ConsumeToken();

			if (m_CurrentToken.Is(TokenType::Colon))
			{
				return ParseLabeledStatement(std::move(id), loc);
			}
		}

		return ParseStmtError();
	}
	case TokenType::LeftBrace:
		return ParseCompoundStatement();
	case TokenType::Semi:
	{
		const auto loc = m_CurrentToken.GetLocation();
		ConsumeToken();
		return m_Sema.ActOnNullStmt(loc);
	}
	case TokenType::Kw_def:
	{
		const auto declBegin = m_CurrentToken.GetLocation();
		SourceLocation declEnd;
		auto decls = ParseDeclaration(context, declEnd);
		return m_Sema.ActOnDeclStmt(move(decls), declBegin, declEnd);
	}
	case TokenType::Kw_if:
		return ParseIfStatement();
	case TokenType::Kw_while:
		return ParseWhileStatement();
	case TokenType::Kw_for:
		return ParseForStatement();
	case TokenType::Kw_goto:
		break;
	case TokenType::Kw_continue:
		return ParseContinueStatement();
	case TokenType::Kw_break:
		return ParseBreakStatement();
	case TokenType::Kw_return:
		return ParseReturnStatement();
	case TokenType::Kw_try:
		break;
	case TokenType::Kw_catch:
		break;
	case TokenType::Dollar:
		ParseCompilerAction([this, &result](natRefPointer<ASTNode> const& node)
		{
			if (result)
			{
				// 多个语句是不允许的
				return true;
			}

			if (const auto decl = static_cast<Declaration::DeclPtr>(node))
			{
				result = m_Sema.ActOnDeclStmt({ decl }, {}, {});
			}
			else
			{
				result = node;
			}

			return false;
		});
		return result;
	case TokenType::Identifier:
	default:
		return ParseExprStatement();
	}

	// TODO
	nat_Throw(NotImplementedException);
}

NatsuLang::Statement::StmtPtr Parser::ParseLabeledStatement(Identifier::IdPtr labelId, SourceLocation labelLoc)
{
	assert(m_CurrentToken.Is(TokenType::Colon));

	const auto colonLoc = m_CurrentToken.GetLocation();

	ConsumeToken();

	auto stmt = ParseStatement();
	if (!stmt)
	{
		stmt = m_Sema.ActOnNullStmt(colonLoc);
	}

	auto labelDecl = m_Sema.LookupOrCreateLabel(std::move(labelId), labelLoc);
	return m_Sema.ActOnLabelStmt(labelLoc, std::move(labelDecl), colonLoc, std::move(stmt));
}

NatsuLang::Statement::StmtPtr Parser::ParseCompoundStatement()
{
	return ParseCompoundStatement(Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::CompoundStmtScope);
}

NatsuLang::Statement::StmtPtr Parser::ParseCompoundStatement(Semantic::ScopeFlags flags)
{
	ParseScope scope{ this, flags };

	assert(m_CurrentToken.Is(TokenType::LeftBrace));
	const auto beginLoc = m_CurrentToken.GetLocation();
	ConsumeBrace();
	const auto braceScope = make_scope([this] { ConsumeBrace(); });

	std::vector<Statement::StmtPtr> stmtVec;

	while (!m_CurrentToken.IsAnyOf({ TokenType::RightBrace, TokenType::Eof }))
	{
		if (auto stmt = ParseStatement())
		{
			stmtVec.emplace_back(std::move(stmt));
		}
	}

	return m_Sema.ActOnCompoundStmt(move(stmtVec), beginLoc, m_CurrentToken.GetLocation());
}

NatsuLang::Statement::StmtPtr Parser::ParseIfStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_if));
	const auto ifLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	if (!m_CurrentToken.Is(TokenType::LeftParen))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::LeftParen)
			.AddArgument(m_CurrentToken.GetType());
		return ParseStmtError();
	}

	ParseScope ifScope{ this, Semantic::ScopeFlags::ControlScope };

	auto cond = ParseParenExpression();
	if (!cond)
	{
		return ParseStmtError();
	}

	const auto thenLoc = m_CurrentToken.GetLocation();
	Statement::StmtPtr thenStmt, elseStmt;

	{
		ParseScope thenScope{ this, Semantic::ScopeFlags::DeclarableScope };
		thenStmt = ParseStatement();
	}
	
	SourceLocation elseLoc, elseStmtLoc;

	if (m_CurrentToken.Is(TokenType::Kw_else))
	{
		elseLoc = m_CurrentToken.GetLocation();

		ConsumeToken();

		elseStmtLoc = m_CurrentToken.GetLocation();

		ParseScope thenScope{ this, Semantic::ScopeFlags::DeclarableScope };
		elseStmt = ParseStatement();
	}

	ifScope.ExplicitExit();

	if (!thenStmt && !elseStmt)
	{
		return ParseStmtError();
	}

	if (!thenStmt)
	{
		thenStmt = m_Sema.ActOnNullStmt(thenLoc);
	}
	
	if (!elseStmt)
	{
		elseStmt = m_Sema.ActOnNullStmt(elseLoc);
	}

	return m_Sema.ActOnIfStmt(ifLoc, std::move(cond), std::move(thenStmt), elseLoc, std::move(elseStmt));
}

NatsuLang::Statement::StmtPtr Parser::ParseWhileStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_while));
	const auto whileLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	if (!m_CurrentToken.Is(TokenType::LeftParen))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::LeftParen)
			.AddArgument(m_CurrentToken.GetType());
		return ParseStmtError();
	}

	ParseScope whileScope{ this, Semantic::ScopeFlags::BreakableScope | Semantic::ScopeFlags::ContinuableScope };

	auto cond = ParseParenExpression();
	if (!cond)
	{
		return ParseStmtError();
	}

	Statement::StmtPtr body;
	{
		ParseScope innerScope{ this, Semantic::ScopeFlags::DeclarableScope };
		body = ParseStatement();
	}
	
	whileScope.ExplicitExit();

	if (!body)
	{
		return ParseStmtError();
	}

	return m_Sema.ActOnWhileStmt(whileLoc, std::move(cond), std::move(body));
}

NatsuLang::Statement::StmtPtr Parser::ParseForStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_for));
	const auto forLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	if (!m_CurrentToken.Is(TokenType::LeftParen))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::LeftParen)
			.AddArgument(m_CurrentToken.GetType());
		return ParseStmtError();
	}

	ParseScope forScope{ this, Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::ControlScope };

	SourceLocation leftParenLoc, rightParenLoc;
	Statement::StmtPtr initPart;
	Expression::ExprPtr condPart;
	Expression::ExprPtr thirdPart;

	{
		leftParenLoc = m_CurrentToken.GetLocation();
		ConsumeParen();

		if (!m_CurrentToken.Is(TokenType::Semi))
		{
			// TODO: 不能知道是否吃掉过分号，分号是必要的吗？
			// 由于如果下一个是分号的话会被吃掉，所以至少不需要担心误判空语句
			initPart = ParseStatement(Declaration::Context::For);
		}
		else
		{
			ConsumeToken();
		}

		// 从这里开始可以 break 和 continue 了
		m_Sema.GetCurrentScope()->AddFlags(Semantic::ScopeFlags::BreakableScope | Semantic::ScopeFlags::ContinuableScope);
		if (!m_CurrentToken.Is(TokenType::Semi))
		{
			condPart = ParseExpression();
			if (m_CurrentToken.Is(TokenType::Semi))
			{
				ConsumeToken();
			}
		}
		else
		{
			ConsumeToken();
		}

		if (!m_CurrentToken.Is(TokenType::RightParen))
		{
			thirdPart = ParseExpression();
		}

		if (!m_CurrentToken.Is(TokenType::RightParen))
		{
			return ParseStmtError();
		}

		rightParenLoc = m_CurrentToken.GetLocation();
		ConsumeParen();
	}

	Statement::StmtPtr body;

	{
		ParseScope innerScope{ this, Semantic::ScopeFlags::DeclarableScope };
		body = ParseStatement();
	}
	
	forScope.ExplicitExit();

	return m_Sema.ActOnForStmt(forLoc, leftParenLoc, std::move(initPart), std::move(condPart), std::move(thirdPart), rightParenLoc, std::move(body));
}

NatsuLang::Statement::StmtPtr Parser::ParseContinueStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_continue));
	const auto loc = m_CurrentToken.GetLocation();
	ConsumeToken();
	return m_Sema.ActOnContinueStmt(loc, m_Sema.GetCurrentScope());
}

NatsuLang::Statement::StmtPtr Parser::ParseBreakStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_break));
	const auto loc = m_CurrentToken.GetLocation();
	ConsumeToken();
	return m_Sema.ActOnBreakStmt(loc, m_Sema.GetCurrentScope());
}

NatsuLang::Statement::StmtPtr Parser::ParseReturnStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_return));
	const auto loc = m_CurrentToken.GetLocation();
	ConsumeToken();

	Expression::ExprPtr returnedExpr;
	if (!m_CurrentToken.Is(TokenType::Semi))
	{
		returnedExpr = ParseExpression();
		if (m_CurrentToken.Is(TokenType::Semi))
		{
			ConsumeToken();
		}
	}

	return m_Sema.ActOnReturnStmt(loc, std::move(returnedExpr), m_Sema.GetCurrentScope());
}

NatsuLang::Statement::StmtPtr Parser::ParseExprStatement()
{
	return m_Sema.ActOnExprStmt(ParseExpression());
}

NatsuLang::Expression::ExprPtr Parser::ParseExpression()
{
	return ParseRightOperandOfBinaryExpression(ParseAssignmentExpression());
}

NatsuLang::Expression::ExprPtr Parser::ParseCastExpression()
{
	Expression::ExprPtr result;
	const auto tokenType = m_CurrentToken.GetType();

	switch (tokenType)
	{
	case TokenType::LeftParen:
		result = ParseParenExpression();
		break;
	case TokenType::NumericLiteral:
		result = m_Sema.ActOnNumericLiteral(m_CurrentToken);
		ConsumeToken();
		break;
	case TokenType::CharLiteral:
		result = m_Sema.ActOnCharLiteral(m_CurrentToken);
		ConsumeToken();
		break;
	case TokenType::StringLiteral:
		result = m_Sema.ActOnStringLiteral(m_CurrentToken);
		ConsumeToken();
		break;
	case TokenType::Kw_true:
	case TokenType::Kw_false:
		result = m_Sema.ActOnBooleanLiteral(m_CurrentToken);
		ConsumeToken();
		break;
	case TokenType::Identifier:
	{
		auto id = m_CurrentToken.GetIdentifierInfo();
		result = m_Sema.ActOnIdExpr(m_Sema.GetCurrentScope(), nullptr, std::move(id), m_CurrentToken.Is(TokenType::LeftParen), m_ResolveContext);
		ConsumeToken();
		break;
	}
	case TokenType::PlusPlus:
	case TokenType::MinusMinus:
	case TokenType::Plus:
	case TokenType::Minus:
	case TokenType::Exclaim:
	case TokenType::Tilde:
	{
		const auto loc = m_CurrentToken.GetLocation();
		ConsumeToken();
		result = ParseCastExpression();
		if (!result)
		{
			// TODO: 报告错误
			result = ParseExprError();
			break;
		}

		result = m_Sema.ActOnUnaryOp(m_Sema.GetCurrentScope(), loc, tokenType, std::move(result));
		break;
	}
	case TokenType::Kw_this:
		return m_Sema.ActOnThis(m_CurrentToken.GetLocation());
	case TokenType::Dollar:
		ParseCompilerAction([&result](natRefPointer<ASTNode> const& node)
		{
			if (result)
			{
				// 多个表达式是不允许的
				return true;
			}

			result = node;
			return false;
		});
		break;
	case TokenType::Eof:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpectEOF, m_CurrentToken.GetLocation());
		return ParseExprError();
	default:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
			.AddArgument(tokenType);
		return ParseExprError();
	}

	return ParseAsTypeExpression(ParsePostfixExpressionSuffix(std::move(result)));
}

NatsuLang::Expression::ExprPtr Parser::ParseAsTypeExpression(Expression::ExprPtr operand)
{
	while (m_CurrentToken.Is(TokenType::Kw_as))
	{
		const auto asLoc = m_CurrentToken.GetLocation();

		// 吃掉 as
		ConsumeToken();

		const auto decl = make_ref<Declaration::Declarator>(Declaration::Context::TypeName);
		ParseDeclarator(decl);
		if (!decl->IsValid())
		{
			// TODO: 报告错误
			operand = nullptr;
		}

		auto type = m_Sema.ActOnTypeName(m_Sema.GetCurrentScope(), decl);
		if (!type)
		{
			// TODO: 报告错误
			operand = nullptr;
		}

		if (operand)
		{
			operand = m_Sema.ActOnAsTypeExpr(m_Sema.GetCurrentScope(), std::move(operand), std::move(type), asLoc);
		}
	}

	return std::move(operand);
}

NatsuLang::Expression::ExprPtr Parser::ParseRightOperandOfBinaryExpression(Expression::ExprPtr leftOperand, OperatorPrecedence minPrec)
{
	auto tokenPrec = GetOperatorPrecedence(m_CurrentToken.GetType());
	SourceLocation colonLoc;

	while (true)
	{
		if (tokenPrec < minPrec)
		{
			return std::move(leftOperand);
		}

		auto opToken = m_CurrentToken;
		ConsumeToken();

		Expression::ExprPtr ternaryMiddle;
		if (tokenPrec == OperatorPrecedence::Conditional)
		{
			ternaryMiddle = ParseAssignmentExpression();
			if (!ternaryMiddle)
			{
				// TODO: 报告错误
				leftOperand = nullptr;
			}

			if (!m_CurrentToken.Is(TokenType::Colon))
			{
				// TODO: 报告可能缺失的':'记号
			}

			colonLoc = m_CurrentToken.GetLocation();
			ConsumeToken();
		}

		auto rightOperand = tokenPrec <= OperatorPrecedence::Conditional ? ParseAssignmentExpression() : ParseCastExpression();
		if (!rightOperand)
		{
			// TODO: 报告错误
			leftOperand = nullptr;
		}

		auto prevPrec = tokenPrec;
		tokenPrec = GetOperatorPrecedence(m_CurrentToken.GetType());

		const auto isRightAssoc = prevPrec == OperatorPrecedence::Assignment || prevPrec == OperatorPrecedence::Conditional;

		if (prevPrec < tokenPrec || (prevPrec == tokenPrec && isRightAssoc))
		{
			rightOperand = ParseRightOperandOfBinaryExpression(std::move(rightOperand),
				static_cast<OperatorPrecedence>(static_cast<std::underlying_type_t<OperatorPrecedence>>(prevPrec) + !isRightAssoc));
			if (!rightOperand)
			{
				// TODO: 报告错误
				leftOperand = nullptr;
			}

			tokenPrec = GetOperatorPrecedence(m_CurrentToken.GetType());
		}

		// TODO: 如果之前的分析发现出错的话条件将不会被满足，由于之前已经报告了错误在此可以不进行报告，但必须继续执行以保证分析完整个表达式
		if (leftOperand)
		{
			leftOperand = ternaryMiddle ?
				m_Sema.ActOnConditionalOp(opToken.GetLocation(), colonLoc, std::move(leftOperand), std::move(ternaryMiddle), std::move(rightOperand)) :
				m_Sema.ActOnBinaryOp(m_Sema.GetCurrentScope(), opToken.GetLocation(), opToken.GetType(), std::move(leftOperand), std::move(rightOperand));
		}
	}
}

NatsuLang::Expression::ExprPtr Parser::ParsePostfixExpressionSuffix(Expression::ExprPtr prefix)
{
	while (true)
	{
		switch (m_CurrentToken.GetType())
		{
		case TokenType::LeftSquare:
		{
			const auto lloc = m_CurrentToken.GetLocation();
			ConsumeBracket();
			auto index = ParseExpression();

			if (!m_CurrentToken.Is(TokenType::RightSquare))
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
					.AddArgument(TokenType::RightSquare)
					.AddArgument(m_CurrentToken.GetType());

				return ParseExprError();
			}

			const auto rloc = m_CurrentToken.GetLocation();
			prefix = m_Sema.ActOnArraySubscriptExpr(m_Sema.GetCurrentScope(), std::move(prefix), lloc, std::move(index), rloc);
			ConsumeBracket();

			break;
		}
		case TokenType::LeftParen:
		{
			if (!prefix)
			{
				return ParseExprError();
			}

			std::vector<Expression::ExprPtr> argExprs;
			std::vector<SourceLocation> commaLocs;

			const auto lloc = m_CurrentToken.GetLocation();
			ConsumeParen();

			if (!m_CurrentToken.Is(TokenType::RightParen) && !ParseExpressionList(argExprs, commaLocs))
			{
				// TODO: 报告错误
				return ParseExprError();
			}

			if (!m_CurrentToken.Is(TokenType::RightParen))
			{
				// TODO: 报告错误
				return ParseExprError();
			}

			ConsumeParen();

			prefix = m_Sema.ActOnCallExpr(m_Sema.GetCurrentScope(), std::move(prefix), lloc, from(argExprs), m_CurrentToken.GetLocation());

			break;
		}
		case TokenType::Period:
		{
			const auto periodLoc = m_CurrentToken.GetLocation();
			ConsumeToken();
			Identifier::IdPtr unqualifiedId;
			if (!ParseUnqualifiedId(unqualifiedId))
			{
				return ParseExprError();
			}

			prefix = m_Sema.ActOnMemberAccessExpr(m_Sema.GetCurrentScope(), std::move(prefix), periodLoc, nullptr, unqualifiedId);

			break;
		}
		case TokenType::PlusPlus:
		case TokenType::MinusMinus:
			prefix = m_Sema.ActOnPostfixUnaryOp(m_Sema.GetCurrentScope(), m_CurrentToken.GetLocation(), m_CurrentToken.GetType(), std::move(prefix));
			ConsumeToken();
			break;
		default:
			return std::move(prefix);
		}
	}
}

NatsuLang::Expression::ExprPtr Parser::ParseConstantExpression()
{
	return ParseRightOperandOfBinaryExpression(ParseCastExpression(), OperatorPrecedence::Conditional);
}

NatsuLang::Expression::ExprPtr Parser::ParseAssignmentExpression()
{
	if (m_CurrentToken.Is(TokenType::Kw_throw))
	{
		return ParseThrowExpression();
	}

	return ParseRightOperandOfBinaryExpression(ParseCastExpression());
}

NatsuLang::Expression::ExprPtr Parser::ParseThrowExpression()
{
	assert(m_CurrentToken.Is(TokenType::Kw_throw));
	const auto throwLocation = m_CurrentToken.GetLocation();
	ConsumeToken();

	switch (m_CurrentToken.GetType())
	{
	case TokenType::Semi:
	case TokenType::RightParen:
	case TokenType::RightSquare:
	case TokenType::RightBrace:
	case TokenType::Colon:
	case TokenType::Comma:
		return m_Sema.ActOnThrow(m_Sema.GetCurrentScope(), throwLocation, {});
	case TokenType::Eof:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpectEOF, m_CurrentToken.GetLocation());
		return ParseExprError();
	default:
		auto expr = ParseAssignmentExpression();
		if (!expr)
		{
			return ParseExprError();
		}
		return m_Sema.ActOnThrow(m_Sema.GetCurrentScope(), throwLocation, std::move(expr));
	}
}

NatsuLang::Expression::ExprPtr Parser::ParseParenExpression()
{
	assert(m_CurrentToken.Is(TokenType::LeftParen));

	ConsumeParen();
	auto ret = ParseExpression();
	if (!m_CurrentToken.Is(TokenType::RightParen))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::RightParen)
			.AddArgument(m_CurrentToken.GetType());
	}
	else
	{
		ConsumeParen();
	}
	
	return ret;
}

nBool Parser::ParseUnqualifiedId(Identifier::IdPtr& result)
{
	if (!m_CurrentToken.Is(TokenType::Identifier))
	{
		return false;
	}

	result = m_CurrentToken.GetIdentifierInfo();
	ConsumeToken();
	return true;
}

nBool Parser::ParseExpressionList(std::vector<Expression::ExprPtr>& exprs, std::vector<SourceLocation>& commaLocs)
{
	while (true)
	{
		auto expr = ParseAssignmentExpression();
		if (!expr)
		{
			SkipUntil({ TokenType::RightParen }, true);
			return false;
		}
		exprs.emplace_back(std::move(expr));
		if (!m_CurrentToken.Is(TokenType::Comma))
		{
			break;
		}
		commaLocs.emplace_back(m_CurrentToken.GetLocation());
		ConsumeToken();
	}

	return true;
}

// declarator:
//	[identifier] [: type] [initializer]
void Parser::ParseDeclarator(Declaration::DeclaratorPtr const& decl, nBool skipIdentifier)
{
	const auto context = decl->GetContext();
	if (!skipIdentifier)
	{
		if (m_CurrentToken.Is(TokenType::Identifier))
		{
			decl->SetIdentifier(m_CurrentToken.GetIdentifierInfo());
			ConsumeToken();
		}
		else if (context != Declaration::Context::Prototype && context != Declaration::Context::TypeName)
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
			return;
		}
	}

	// TODO: 验证其他上下文下是否需要延迟分析
	if (m_Sema.GetCurrentPhase() == Semantic::Sema::Phase::Phase1 && context != Declaration::Context::Prototype && context != Declaration::Context::TypeName)
	{
		skipTypeAndInitializer(decl);
	}
	else
	{
		// (: int)也可以？
		if (m_CurrentToken.Is(TokenType::Colon) || ((context == Declaration::Context::Prototype || context == Declaration::Context::TypeName) && !decl->GetIdentifier()))
		{
			ParseType(decl);
		}

		// 声明函数原型时也可以指定initializer？
		if (context != Declaration::Context::TypeName &&
			decl->GetStorageClass() != Specifier::StorageClass::Extern &&
			m_CurrentToken.IsAnyOf({ TokenType::Equal, TokenType::LeftBrace }))
		{
			ParseInitializer(decl);
		}
	}
}

// specifier-seq:
//	specifier-seq specifier
// specifier:
//	storage-class-specifier
//	access-specifier
void Parser::ParseSpecifier(Declaration::DeclaratorPtr const& decl)
{
	while (true)
	{
		switch (m_CurrentToken.GetType())
		{
		case TokenType::Kw_extern:
			if (decl->GetStorageClass() != Specifier::StorageClass::None)
			{
				// TODO: 报告错误：多个存储类说明符
			}
			decl->SetStorageClass(Specifier::StorageClass::Extern);
			break;
		case TokenType::Kw_static:
			if (decl->GetStorageClass() != Specifier::StorageClass::None)
			{
				// TODO: 报告错误：多个存储类说明符
			}
			decl->SetStorageClass(Specifier::StorageClass::Static);
			break;
		case TokenType::Kw_public:
			if (decl->GetAccessibility() != Specifier::Access::None)
			{
				// TODO: 报告错误：多个访问说明符
			}
			decl->SetAccessibility(Specifier::Access::Public);
			break;
		case TokenType::Kw_protected:
			if (decl->GetAccessibility() != Specifier::Access::None)
			{
				// TODO: 报告错误：多个访问说明符
			}
			decl->SetAccessibility(Specifier::Access::Protected);
			break;
		case TokenType::Kw_internal:
			if (decl->GetAccessibility() != Specifier::Access::None)
			{
				// TODO: 报告错误：多个访问说明符
			}
			decl->SetAccessibility(Specifier::Access::Internal);
			break;
		case TokenType::Kw_private:
			if (decl->GetAccessibility() != Specifier::Access::None)
			{
				// TODO: 报告错误：多个访问说明符
			}
			decl->SetAccessibility(Specifier::Access::Private);
			break;
		default:
			// 不是错误
			return;
		}

		ConsumeToken();
	}
}

// type-specifier:
//	[auto]
//	typeof-specifier
//	type-identifier
void Parser::ParseType(Declaration::DeclaratorPtr const& decl)
{
	const auto context = decl->GetContext();

	if (!m_CurrentToken.Is(TokenType::Colon))
	{
		if (context != Declaration::Context::Prototype && context != Declaration::Context::TypeName)
		{
			// 在非声明函数原型的上下文中不显式写出类型，视为隐含auto
			// auto的声明符在指定initializer之后决定实际类型
			return;
		}
	}
	else
	{
		ConsumeToken();
	}

	const auto tokenType = m_CurrentToken.GetType();
	switch (tokenType)
	{
	case TokenType::Identifier:
	{
		// 普通或数组类型

		auto type = m_Sema.GetTypeName(m_CurrentToken.GetIdentifierInfo(), m_CurrentToken.GetLocation(), m_Sema.GetCurrentScope(), nullptr);
		if (!type)
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedTypeSpecifierGot, m_CurrentToken.GetLocation())
				.AddArgument(m_CurrentToken.GetIdentifierInfo());
			return;
		}

		decl->SetType(std::move(type));

		ConsumeToken();
		while (m_CurrentToken.Is(TokenType::Period))
		{
			ConsumeToken();

			if (!m_CurrentToken.Is(TokenType::Identifier))
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
					.AddArgument(TokenType::Identifier)
					.AddArgument(m_CurrentToken.GetType());
				// TODO: 错误恢复
			}

			// TODO: 嵌套类型
			nat_Throw(NotImplementedException);
		}

		break;
	}
	case TokenType::LeftParen:
	{
		// 函数类型或者括号类型
		ParseFunctionType(decl);

		break;
	}
	case TokenType::RightParen:
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::RightParen);
		ConsumeParen();

		return;
	}
	case TokenType::Kw_typeof:
	{
		ParseTypeOfType(decl);

		break;
	}
	case TokenType::Eof:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpectEOF, m_CurrentToken.GetLocation());
		break;
	case TokenType::Dollar:
		ParseCompilerAction([&decl](natRefPointer<ASTNode> const& node)
		{
			if (decl->GetType())
			{
				// 多个类型是不允许的
				return true;
			}

			decl->SetType(node);
			return false;
		});
		break;
	default:
	{
		const auto builtinClass = Type::BuiltinType::GetBuiltinClassFromTokenType(tokenType);
		if (builtinClass == Type::BuiltinType::Invalid)
		{
			// 对于无效类型的处理
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedTypeSpecifierGot, m_CurrentToken.GetLocation())
				.AddArgument(tokenType);
			return;
		}
		decl->SetType(m_Sema.GetASTContext().GetBuiltinType(builtinClass));
		ConsumeToken();
		break;
	}
	}

	// 即使类型不是数组尝试Parse也不会错误
	ParseArrayType(decl);
}

void Parser::ParseTypeOfType(Declaration::DeclaratorPtr const& decl)
{
	assert(m_CurrentToken.Is(TokenType::Kw_typeof));
	ConsumeToken();
	if (!m_CurrentToken.Is(TokenType::LeftParen))
	{
		// TODO: 报告错误
		return;
	}

	auto expr = ParseParenExpression();
	if (!expr)
	{
		// TODO: 报告错误
		return;
	}

	auto exprType = expr->GetExprType();
	decl->SetType(m_Sema.ActOnTypeOfType(std::move(expr), std::move(exprType)));
}

void Parser::ParseParenType(Declaration::DeclaratorPtr const& decl)
{
	assert(m_CurrentToken.Is(TokenType::LeftParen));
	// 吃掉左括号
	ConsumeParen();

	ParseType(decl);
	if (!decl->GetType())
	{
		if (m_CurrentToken.Is(TokenType::Identifier))
		{
			// 函数类型
			ParseFunctionType(decl);
			return;
		}
		
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
	}
}

void Parser::ParseFunctionType(Declaration::DeclaratorPtr const& decl)
{
	assert(m_CurrentToken.Is(TokenType::LeftParen));
	ConsumeParen();

	std::vector<Declaration::DeclaratorPtr> paramDecls;

	auto mayBeParenType = true;

	ParseScope prototypeScope{ this, Semantic::ScopeFlags::FunctionDeclarationScope | Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::FunctionPrototypeScope };

	if (!m_CurrentToken.Is(TokenType::RightParen))
	{
		while (true)
		{
			auto param = make_ref<Declaration::Declarator>(Declaration::Context::Prototype);
			ParseDeclarator(param);
			if (mayBeParenType && param->GetIdentifier() || !param->GetType())
			{
				mayBeParenType = false;
			}

			if (param->IsValid())
			{
				paramDecls.emplace_back(param);
			}
			else
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedDeclarator, m_CurrentToken.GetLocation());
			}

			if (!param->GetType() && !param->GetInitializer())
			{
				// TODO: 报告错误：参数的类型和初始化器至少要存在一个
			}

			if (m_CurrentToken.Is(TokenType::RightParen))
			{
				ConsumeParen();
				break;
			}

			mayBeParenType = false;

			if (!m_CurrentToken.Is(TokenType::Comma))
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
					.AddArgument(TokenType::Comma)
					.AddArgument(m_CurrentToken.GetType());
			}
			else
			{
				ConsumeToken();
			}
		}
	}
	else
	{
		mayBeParenType = false;
		ConsumeParen();
	}

	// 读取完函数参数信息，开始读取返回类型

	// 如果不是->且只有一个无名称参数说明是普通的括号类型
	if (!m_CurrentToken.Is(TokenType::Arrow))
	{
		if (mayBeParenType)
		{
			// 是括号类型，但是我们已经把Token处理完毕了。。。
			*decl = std::move(*paramDecls[0]);
			return;
		}

		// 以后会加入元组或匿名类型的支持吗？
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::Arrow)
			.AddArgument(m_CurrentToken.GetType());
	}

	ConsumeToken();

	const auto retType = make_ref<Declaration::Declarator>(Declaration::Context::Prototype);
	ParseType(retType);

	decl->SetType(m_Sema.BuildFunctionType(retType->GetType(), from(paramDecls)
		.select([](Declaration::DeclaratorPtr const& paramDecl)
				{
					return paramDecl->GetType();
				})));

	decl->SetParams(from(paramDecls)
		.select([this](Declaration::DeclaratorPtr const& paramDecl)
				{
					return m_Sema.ActOnParamDeclarator(m_Sema.GetCurrentScope(), paramDecl);
				}));
}

void Parser::ParseArrayType(Declaration::DeclaratorPtr const& decl)
{
	while (m_CurrentToken.Is(TokenType::LeftSquare))
	{
		ConsumeBracket();
		auto countExpr = ParseConstantExpression();
		decl->SetType(m_Sema.GetASTContext().GetArrayType(decl->GetType(), std::move(countExpr)));
		if (!m_CurrentToken.Is(TokenType::RightSquare))
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				.AddArgument(TokenType::RightSquare)
				.AddArgument(m_CurrentToken.GetType());
		}
		ConsumeAnyToken();
	}
}

// initializer:
//	= expression
//	= { expression-list }
//	compound-statement
void Parser::ParseInitializer(Declaration::DeclaratorPtr const& decl)
{
	if (m_CurrentToken.Is(TokenType::Equal))
	{
		ConsumeToken();
		decl->SetInitializer(ParseExpression());
		return;
	}

	if (m_CurrentToken.Is(TokenType::LeftBrace))
	{
		if (decl->GetType()->GetType() != Type::Type::Function)
		{
			// TODO: 报告错误
			return;
		}

		ParseScope bodyScope{ this, Semantic::ScopeFlags::FunctionScope | Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::CompoundStmtScope };
		auto funcDecl = m_Sema.ActOnStartOfFunctionDef(m_Sema.GetCurrentScope(), decl);
		decl->SetDecl(ParseFunctionBody(std::move(funcDecl), bodyScope));
	}

	// 不是 initializer，返回
}

nBool Parser::SkipUntil(std::initializer_list<NatsuLang::Lex::TokenType> list, nBool dontConsume, std::vector<Lex::Token>* skippedTokens)
{
	// 特例，如果调用者只是想跳到文件结尾，我们不需要再另外判断其他信息
	if (list.size() == 1 && *list.begin() == TokenType::Eof)
	{
		while (!m_CurrentToken.Is(TokenType::Eof))
		{
			skipToken(skippedTokens);
		}
		return true;
	}

	while (true)
	{
		const auto currentType = m_CurrentToken.GetType();
		for (auto type : list)
		{
			if (currentType == type)
			{
				if (!dontConsume)
				{
					skipToken(skippedTokens);
				}

				return true;
			}
		}

		switch (currentType)
		{
		case TokenType::Eof:
			return false;
		case TokenType::LeftParen:
			skipToken(skippedTokens);
			SkipUntil({ TokenType::RightParen }, false, skippedTokens);
			break;
		case TokenType::LeftSquare:
			skipToken(skippedTokens);
			SkipUntil({ TokenType::RightSquare }, false, skippedTokens);
			break;
		case TokenType::LeftBrace:
			skipToken(skippedTokens);
			SkipUntil({ TokenType::RightBrace }, false, skippedTokens);
			break;
		case TokenType::RightParen:	// 可能的不匹配括号，下同
			skipToken(skippedTokens);
			break;
		case TokenType::RightSquare:
			skipToken(skippedTokens);
			break;
		case TokenType::RightBrace:
			skipToken(skippedTokens);
			break;
		default:
			skipToken(skippedTokens);
			break;
		}
	}
}

void Parser::pushCachedTokens(std::vector<Token> tokens)
{
	m_Preprocessor.PushCachedTokens(move(tokens));
	ConsumeToken();
}

void Parser::popCachedTokens()
{
	m_Preprocessor.PopCachedTokens();
}

void Parser::skipTypeAndInitializer(Declaration::DeclaratorPtr const& decl)
{
	std::vector<Token> cachedTokens;

	if (m_CurrentToken.Is(TokenType::Colon))
	{
		skipToken(&cachedTokens);
		skipType(&cachedTokens);
	}

	if (m_CurrentToken.Is(TokenType::Equal))
	{
		skipToken(&cachedTokens);
		if (m_CurrentToken.Is(TokenType::LeftBrace))
		{
			skipToken(&cachedTokens);
			SkipUntil({ TokenType::RightBrace }, false, &cachedTokens);
		}
		else
		{
			skipExpression(&cachedTokens);
		}
	}
	else if (m_CurrentToken.Is(TokenType::LeftBrace))
	{
		skipToken(&cachedTokens);
		SkipUntil({ TokenType::RightBrace }, false, &cachedTokens);
	}
	else
	{
		// TODO: 报告错误
	}

	decl->SetCachedTokens(std::move(cachedTokens));
}

void Parser::ResolveDeclarator(Declaration::DeclaratorPtr const& decl)
{
	assert(m_Sema.GetCurrentPhase() == Semantic::Sema::Phase::Phase2 && m_ResolveContext);
	assert(!decl->GetType() && !decl->GetInitializer());

	pushCachedTokens(decl->GetAndClearCachedTokens());
	const auto tokensScope = make_scope([this]
	{
		popCachedTokens();
	});

	m_ResolveContext->StartResolvingDeclarator(decl);
	const auto resolveContextScope = make_scope([this, &decl]
	{
		m_ResolveContext->EndResolvingDeclarator(decl);
	});

	const auto curScope = m_Sema.GetCurrentScope();
	const auto recoveryScope = make_scope([this, curScope]
	{
		m_Sema.SetCurrentScope(curScope);
	});
	m_Sema.SetCurrentScope(decl->GetDeclarationScope());

	ParseDeclarator(decl, true);
}

void Parser::skipType(std::vector<Token>* skippedTokens)
{
	const auto tokenType = m_CurrentToken.GetType();
	switch (tokenType)
	{
	case TokenType::Identifier:
		skipToken(skippedTokens);
		while (m_CurrentToken.Is(TokenType::Period))
		{
			skipToken(skippedTokens);
			// 应当是 unqualified-id
			skipToken(skippedTokens);
		}

		break;
	case TokenType::LeftParen:
		skipToken(skippedTokens);
		SkipUntil({ TokenType::RightParen }, false, skippedTokens);

		if (m_CurrentToken.Is(TokenType::Arrow))
		{
			// 是函数类型
			skipToken(skippedTokens);
			skipType(skippedTokens);
		}

		break;
	case TokenType::Kw_typeof:
		skipToken(skippedTokens);
		if (m_CurrentToken.Is(TokenType::LeftParen))
		{
			skipToken(skippedTokens);
		}
		SkipUntil({ TokenType::RightParen }, false, skippedTokens);
		break;
	case TokenType::Eof:
		return;
	case TokenType::Dollar:
		skipCompilerAction(skippedTokens);
		while (m_CurrentToken.Is(TokenType::Period))
		{
			skipToken(skippedTokens);
			// 应当是 unqualified-id
			skipToken(skippedTokens);
		}
		break;
	default:
		skipToken(skippedTokens);
		break;
	}

	while (m_CurrentToken.Is(TokenType::LeftSquare))
	{
		skipToken(skippedTokens);
		SkipUntil({ TokenType::RightSquare }, false, skippedTokens);
	}
}

void Parser::skipExpression(std::vector<Token>* skippedTokens)
{
	skipAssignmentExpression(skippedTokens);
	skipRightOperandOfBinaryExpression(skippedTokens);
}

void Parser::skipAssignmentExpression(std::vector<Token>* skippedTokens)
{
	skipCastExpression(skippedTokens);
	skipRightOperandOfBinaryExpression(skippedTokens);
}

void Parser::skipRightOperandOfBinaryExpression(std::vector<Token>* skippedTokens)
{
	while (true)
	{
		const auto prec = GetOperatorPrecedence(m_CurrentToken.GetType());
		skipToken(skippedTokens);
		if (prec == OperatorPrecedence::Unknown)
		{
			break;
		}

		if (prec == OperatorPrecedence::Conditional)
		{
			skipAssignmentExpression(skippedTokens);
			// 缺失的冒号，第二次处理的时候会报错，所以此次就不报错了
			if (m_CurrentToken.Is(TokenType::Colon))
			{
				skipToken(skippedTokens);
			}
		}

		if (prec <= OperatorPrecedence::Conditional)
		{
			skipAssignmentExpression(skippedTokens);
		}
		else
		{
			skipCastExpression(skippedTokens);
		}
	}
}

void Parser::skipCastExpression(std::vector<Token>* skippedTokens)
{
	const auto tokenType = m_CurrentToken.GetType();
	switch (tokenType)
	{
	case TokenType::LeftParen:
		SkipUntil({ TokenType::RightParen }, false, skippedTokens);
		break;
	case TokenType::NumericLiteral:
	case TokenType::CharLiteral:
	case TokenType::StringLiteral:
	case TokenType::Kw_true:
	case TokenType::Kw_false:
	case TokenType::Identifier:
	case TokenType::Kw_this:
		skipToken(skippedTokens);
		break;
	case TokenType::PlusPlus:
	case TokenType::MinusMinus:
	case TokenType::Plus:
	case TokenType::Minus:
	case TokenType::Exclaim:
	case TokenType::Tilde:
	{
		skipToken(skippedTokens);
		skipCastExpression(skippedTokens);
		break;
	}
	case TokenType::Dollar:
		skipCompilerAction(skippedTokens);
		break;
	case TokenType::Eof:
		return;
	default:
		break;
	}

	skipPostfixExpressionSuffix(skippedTokens);
	skipAsTypeExpression(skippedTokens);
}

void Parser::skipPostfixExpressionSuffix(std::vector<Token>* skippedTokens)
{
	while (true)
	{
		switch (m_CurrentToken.GetType())
		{
		case TokenType::LeftSquare:
			SkipUntil({ TokenType::RightSquare }, false, skippedTokens);
			break;
		case TokenType::LeftParen:
			SkipUntil({ TokenType::RightParen }, false, skippedTokens);
			break;
		case TokenType::Period:
			skipToken(skippedTokens);
			// 应当是 unqualified-id
			skipToken(skippedTokens);
			break;
		case TokenType::PlusPlus:
		case TokenType::MinusMinus:
			skipToken(skippedTokens);
			break;
		default:
			return;
		}
	}
}

void Parser::skipAsTypeExpression(std::vector<Token>* skippedTokens)
{
	assert(m_CurrentToken.Is(TokenType::Kw_as));

	skipToken(skippedTokens);
	skipType(skippedTokens);
}

void Parser::skipCompilerAction(std::vector<Token>* skippedTokens)
{
	assert(m_CurrentToken.Is(TokenType::Dollar));

	SkipUntil({ TokenType::LeftParen }, false, skippedTokens);
	SkipUntil({ TokenType::RightParen }, false, skippedTokens);
}

Parser::ParseScope::ParseScope(Parser* self, Semantic::ScopeFlags flags)
	: m_Self{ self }
{
	assert(m_Self);

	m_Self->m_Sema.PushScope(flags);
}

Parser::ParseScope::~ParseScope()
{
	ExplicitExit();
}

void Parser::ParseScope::ExplicitExit()
{
	if (m_Self)
	{
		m_Self->m_Sema.PopScope();
		m_Self = nullptr;
	}
}

void NatsuLang::ParseAST(Preprocessor& pp, ASTContext& astContext, natRefPointer<ASTConsumer> astConsumer)
{
	Semantic::Sema sema{ pp, astContext, astConsumer };
	Parser parser{ pp, sema };

	ParseAST(parser);
}

void NatsuLang::ParseAST(Parser& parser)
{
	auto& sema = parser.GetSema();
	auto const& consumer = sema.GetASTConsumer();

	std::vector<Declaration::DeclPtr> decls;

	while (!parser.ParseTopLevelDecl(decls))
	{
		if (!consumer->HandleTopLevelDecl(from(decls)))
		{
			return;
		}
	}

	consumer->HandleTranslationUnit(sema.GetASTContext());
}

void NatsuLang::EndParsingAST(Parser& parser)
{
	auto& sema = parser.GetSema();
	auto const& consumer = sema.GetASTConsumer();
	std::vector<Declaration::DeclPtr> decls;

	// 进入2阶段，解析所有声明符
	parser.DivertPhase(decls);

	consumer->HandleTopLevelDecl(from(decls));

	consumer->HandleTranslationUnit(sema.GetASTContext());
}
