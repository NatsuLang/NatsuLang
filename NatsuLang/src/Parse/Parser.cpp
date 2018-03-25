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
using namespace Syntax;
using namespace Lex;
using namespace Diag;

void ResolveContext::StartResolvingDeclarator(Declaration::DeclaratorPtr decl)
{
	m_ResolvingDeclarators.emplace(std::move(decl));
}

void ResolveContext::EndResolvingDeclarator(Declaration::DeclaratorPtr const& decl)
{
	m_ResolvedDeclarators.emplace(decl);
	m_ResolvingDeclarators.erase(decl);
}

ResolveContext::ResolvingState ResolveContext::GetDeclaratorResolvingState(
	Declaration::DeclaratorPtr const& decl) const noexcept
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
	: m_Preprocessor{ preprocessor }, m_Diag{ preprocessor.GetDiag() }, m_Sema{ sema }, m_ParenCount{}, m_BracketCount{},
	  m_BraceCount{}
{
	ConsumeToken();
}

Parser::~Parser()
{
}

Preprocessor& Parser::GetPreprocessor() const noexcept
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
		pushCachedTokens(move(skippedTopLevelCompilerAction));
		const auto compilerActionScope = make_scope([this]
		{
			popCachedTokens();
		});

		ParseCompilerAction(Declaration::Context::Global, [&decls](natRefPointer<ASTNode> const& astNode)
		{
			if (auto decl = static_cast<Declaration::DeclPtr>(astNode))
			{
				decls.emplace_back(std::move(decl));
				return false;
			}

			// TODO: 报告错误：编译器动作插入了声明以外的 AST
			return true;
		});

		if (m_CurrentToken.Is(TokenType::Semi))
		{
			ConsumeToken();
		}
		else
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				.AddArgument(TokenType::Semi)
				.AddArgument(m_CurrentToken.GetType());
		}
	}

	m_SkippedTopLevelCompilerActions.clear();

	for (auto declPtr : m_Sema.GetCachedDeclarators())
	{
		if (m_ResolveContext->GetDeclaratorResolvingState(declPtr) == ResolveContext::ResolvingState::Unknown)
		{
			ResolveDeclarator(std::move(declPtr));
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
		decls = { ParseModuleDecl() };
		return false;
	case TokenType::Eof:
		return true;
	case TokenType::Dollar:
	{
		std::vector<Token> cachedTokens;
		SkipUntil({ TokenType::Semi }, false, &cachedTokens);
		m_SkippedTopLevelCompilerActions.emplace_back(move(cachedTokens));
		return false;
	}
	case TokenType::Kw_unsafe:
	{
		ConsumeToken();

		const auto curScope = m_Sema.GetCurrentScope();
		curScope->AddFlags(Semantic::ScopeFlags::UnsafeScope);
		const auto scope = make_scope([curScope]
		{
			curScope->RemoveFlags(Semantic::ScopeFlags::UnsafeScope);
		});

		if (!m_CurrentToken.Is(TokenType::LeftBrace))
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				  .AddArgument(TokenType::LeftBrace)
				  .AddArgument(m_CurrentToken.GetType());
			// 假设漏写了左大括号，继续分析
		}
		else
		{
			ConsumeBrace();
		}

		std::vector<Declaration::DeclPtr> curResult;
		while (!m_CurrentToken.Is(TokenType::RightBrace))
		{
			// TODO: 允许不安全声明
			const auto encounteredEof = ParseTopLevelDecl(curResult);

			decls.insert(decls.end(), std::make_move_iterator(curResult.begin()), std::make_move_iterator(curResult.end()));

			if (encounteredEof)
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpectEOF, m_CurrentToken.GetLocation());
				return true;
			}
		}

		ConsumeBrace();
		return false;
	}
	default:
		break;
	}

	decls = ParseExternalDeclaration();
	return false;
}

std::vector<Declaration::DeclPtr> Parser::ParseExternalDeclaration()
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
		return { ParseClassDeclaration() };
	default:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
			  .AddArgument(m_CurrentToken.GetType());

		// 吃掉 1 个 Token 以保证不会死循环
		ConsumeToken();
		return {};
	}
}

// compiler-action:
//	'$' compiler-action-name ['(' compiler-action-argument-list ')'] [compiler-action-argument] ['{' compiler-action-argument-seq '}'] ;
// compiler-action-name:
//	[compiler-action-namespace-specifier] compiler-action-id
// compiler-action-namespace-specifier:
//	compiler-action-namespace-id '.'
//	compiler-action-namespace-specifier compiler-action-namespace-id '.'
void Parser::ParseCompilerAction(Declaration::Context context, std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	assert(m_CurrentToken.Is(TokenType::Dollar));
	ConsumeToken();

	const auto action = ParseCompilerActionName();

	if (!action)
	{
		return;
	}

	action->StartAction(CompilerActionContext{ *this });
	const auto scope = make_scope([&action, &output]
	{
		action->EndAction(output);
	});

	std::size_t argCount = 0;
	if (m_CurrentToken.Is(TokenType::LeftParen))
	{
		argCount = ParseCompilerActionArgumentList(action, context, argCount);
	}

	if (!m_CurrentToken.IsAnyOf({ TokenType::Semi, TokenType::LeftBrace }))
	{
		argCount += ParseCompilerActionArgument(action, context, argCount);
	}

	if (m_CurrentToken.Is(TokenType::LeftBrace))
	{
		ParseCompilerActionArgumentSequence(action, context, argCount);
	}
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

nBool Parser::ParseCompilerActionArgument(natRefPointer<ICompilerAction> const& action, Declaration::Context context, std::size_t startIndex)
{
	const auto requirement = action->GetArgumentRequirement();

	const auto argType = requirement->GetExpectedArgumentType(startIndex);

	// TODO: 替换成正儿八经的实现
	// 禁止匹配过程中的错误报告
	m_Diag.EnableDiag(false);
	const auto scope = make_scope([this]
	{
		m_Diag.EnableDiag(true);
	});

	if (argType == CompilerActionArgumentType::None)
	{
		return false;
	}

	// 最优先尝试匹配标识符
	if (HasFlags(argType, CompilerActionArgumentType::Identifier) && m_CurrentToken.Is(TokenType::Identifier))
	{
		action->AddArgument(m_Sema.ActOnCompilerActionIdentifierArgument(m_CurrentToken.GetIdentifierInfo()));
		ConsumeToken();
		return true;
	}

	// 记录状态以便匹配失败时还原
	const auto memento = m_Preprocessor.SaveToMemento();

	if (HasFlags(argType, CompilerActionArgumentType::Type))
	{
		const auto typeDecl = make_ref<Declaration::Declarator>(Declaration::Context::TypeName);
		ParseType(typeDecl);
		const auto type = typeDecl->GetType();
		if (type)
		{
			action->AddArgument(type);
			return true;
		}
	}

	// 匹配类型失败了，还原 Preprocessor 状态
	m_Preprocessor.RestoreFromMemento(memento);

	if (HasFlags(argType, CompilerActionArgumentType::Declaration))
	{
		SourceLocation end;
		const auto decl = ParseDeclaration(context, end);
		if (!decl.empty())
		{
			assert(decl.size() == 1);
			action->AddArgument(decl.front());

			return true;
		}
	}

	// 匹配声明失败了，还原 Preprocessor 状态
	m_Preprocessor.RestoreFromMemento(memento);

	assert(HasFlags(argType, CompilerActionArgumentType::Statement) &&
		"argType has only set flag Optional");
	const auto stmt = ParseStatement(context, true);
	if (stmt)
	{
		action->AddArgument(stmt);
		return true;
	}

	// 匹配全部失败，还原 Preprocessor 状态并报告错误
	m_Preprocessor.RestoreFromMemento(memento);
	m_Diag.EnableDiag(true);
	m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
		.AddArgument(m_CurrentToken.GetType());
	return false;
}

std::size_t Parser::ParseCompilerActionArgumentList(natRefPointer<ICompilerAction> const& action, Declaration::Context context, std::size_t startIndex)
{
	assert(m_CurrentToken.Is(TokenType::LeftParen));
	ConsumeParen();

	const auto requirement = action->GetArgumentRequirement();

	auto i = startIndex;
	for (;; ++i)
	{
		const auto argType = requirement->GetExpectedArgumentType(i);

		if (m_CurrentToken.Is(TokenType::RightParen))
		{
			if (argType == CompilerActionArgumentType::None || HasFlags(argType, CompilerActionArgumentType::Optional))
			{
				ConsumeParen();
				break;
			}

			// TODO: 参数过少，或者之后的参数由其他形式补充
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

		if (!ParseCompilerActionArgument(action, context, i))
		{
			// 匹配失败，报告错误
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
				.AddArgument(m_CurrentToken.GetType());
			break;
		}

		if (m_CurrentToken.Is(TokenType::Comma))
		{
			ConsumeToken();
		}
	}

	return i;
}

std::size_t Parser::ParseCompilerActionArgumentSequence(natRefPointer<ICompilerAction> const& action, Declaration::Context context, std::size_t startIndex)
{
	assert(m_CurrentToken.Is(TokenType::LeftBrace));
	ConsumeBrace();

	const auto requirement = action->GetArgumentRequirement();

	auto i = startIndex;
	for (;; ++i)
	{
		const auto argType = requirement->GetExpectedArgumentType(i);

		if (m_CurrentToken.Is(TokenType::RightBrace))
		{
			if (argType == CompilerActionArgumentType::None || HasFlags(argType, CompilerActionArgumentType::Optional))
			{
				ConsumeBrace();
				break;
			}

			// TODO: 参数过少，或者之后的参数由其他形式补充
			break;
		}

		if (!ParseCompilerActionArgument(action, context, i))
		{
			// 匹配失败，报告错误
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
				.AddArgument(m_CurrentToken.GetType());
			break;
		}
	}

	return i;
}

// class-declaration:
//	'class' [specifier-seq] identifier '{' [member-specification] '}'
Declaration::DeclPtr Parser::ParseClassDeclaration()
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
		return ParseDeclError();
	}

	const auto classId = m_CurrentToken.GetIdentifierInfo();
	const auto classIdLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	auto classDecl = m_Sema.ActOnTag(m_Sema.GetCurrentScope(), Type::TagType::TagTypeClass::Class, classKeywordLoc,
									 accessSpecifier, classId, classIdLoc, nullptr);

	ParseMemberSpecification(classKeywordLoc, classDecl);

	return classDecl;
}

// member-specification:
//	member-declaration [member-specification]
void Parser::ParseMemberSpecification(SourceLocation startLoc, Declaration::DeclPtr const& tagDecl)
{
	ParseScope classScope{ this, Semantic::ScopeFlags::ClassScope | Semantic::ScopeFlags::DeclarableScope };

	m_Sema.ActOnTagStartDefinition(m_Sema.GetCurrentScope(), tagDecl);
	const auto tagScope = make_scope([this]
	{
		m_Sema.ActOnTagFinishDefinition();
	});

	if (m_CurrentToken.Is(TokenType::Colon))
	{
		ConsumeToken();
		// TODO: 实现的 Concept 说明
	}

	if (m_CurrentToken.Is(TokenType::LeftBrace))
	{
		ConsumeBrace();
	}
	else
	{
		// 可能缺失的左大括号
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot)
			  .AddArgument(TokenType::LeftBrace)
			  .AddArgument(m_CurrentToken.GetType());
	}

	// 开始成员声明
	while (!m_CurrentToken.Is(TokenType::RightBrace))
	{
		switch (m_CurrentToken.GetType())
		{
		case TokenType::Kw_def:
		{
			SourceLocation declEnd;
			ParseDeclaration(Declaration::Context::Member, declEnd);
			break;
		}
		case TokenType::Eof:
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot)
				  .AddArgument(TokenType::RightBrace)
				  .AddArgument(TokenType::Eof);
			return;
		default:
			break;
		}
	}

	ConsumeBrace();
}

std::vector<Declaration::DeclPtr> Parser::ParseModuleImport()
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

// module-decl:
//	'module' module-name '{' declarations '}'
Declaration::DeclPtr Parser::ParseModuleDecl()
{
	assert(m_CurrentToken.Is(TokenType::Kw_module));

	const auto startLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

	if (!m_CurrentToken.Is(TokenType::Identifier))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			  .AddArgument(TokenType::Identifier)
			  .AddArgument(m_CurrentToken.GetType());
		return {};
	}

	auto moduleName = m_CurrentToken.GetIdentifierInfo();
	auto moduleDecl = m_Sema.ActOnModuleDecl(m_Sema.GetCurrentScope(), startLoc, std::move(moduleName));

	{
		ParseScope moduleScope{ this, Semantic::ScopeFlags::DeclarableScope };
		m_Sema.ActOnStartModule(m_Sema.GetCurrentScope(), moduleDecl);
		const auto scope = make_scope([this]
		{
			m_Sema.ActOnFinishModule();
		});

		ConsumeToken();
		if (m_CurrentToken.Is(TokenType::LeftBrace))
		{
			ConsumeBrace();
		}
		else
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				  .AddArgument(TokenType::LeftBrace)
				  .AddArgument(m_CurrentToken.GetType());
		}

		while (!m_CurrentToken.IsAnyOf({ TokenType::RightBrace, TokenType::Eof }))
		{
			ParseExternalDeclaration();
		}

		if (m_CurrentToken.Is(TokenType::Eof))
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpectEOF, m_CurrentToken.GetLocation());
		}
		else
		{
			ConsumeBrace();
		}
	}

	return moduleDecl;
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
//	special-member-function-declaration
// simple-declaration:
//	'def' [specifier-seq] declarator [;]
// special-member-function-declaration:
//	'def' 'this' ':' '(' [parameter-declaration-list] ')' function-body
//	'def' '~' 'this' ':' '(' ')' function-body
// function-body:
//	compound-statement
std::vector<Declaration::DeclPtr> Parser::ParseDeclaration(Declaration::Context context, SourceLocation& declEnd)
{
	const auto tokenType = m_CurrentToken.GetType();

	switch (tokenType)
	{
	case TokenType::Kw_def:
	{
		// 吃掉 def
		ConsumeToken();

		const auto decl = make_ref<Declaration::Declarator>(context);
		// 这不意味着 specifier 是 declarator 的一部分，至少目前如此
		ParseSpecifier(decl);
		ParseDeclarator(decl);

		auto declaration = m_Sema.HandleDeclarator(m_Sema.GetCurrentScope(), decl);

		if (decl->IsUnresolved())
		{
			return {};
		}

		return { std::move(declaration) };
	}
	default:
		return {};
	}
}

Declaration::DeclPtr Parser::ParseFunctionBody(Declaration::DeclPtr decl, ParseScope& scope)
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

Statement::StmtPtr Parser::ParseStatement(Declaration::Context context, nBool mayBeExpr)
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
	case TokenType::Kw_unsafe:
		ConsumeToken();
		if (!m_CurrentToken.Is(TokenType::LeftBrace))
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				  .AddArgument(TokenType::LeftBrace)
				  .AddArgument(m_CurrentToken.GetType());
			return nullptr;
		}
		return ParseCompoundStatement(
			Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::CompoundStmtScope | Semantic::ScopeFlags::UnsafeScope);
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
		ParseCompilerAction(context, [this, &result](natRefPointer<ASTNode> const& node)
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
		return ParseExprStatement(mayBeExpr);
	}

	// TODO
	nat_Throw(NotImplementedException);
}

Statement::StmtPtr Parser::ParseLabeledStatement(Identifier::IdPtr labelId, SourceLocation labelLoc)
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

Statement::StmtPtr Parser::ParseCompoundStatement()
{
	return ParseCompoundStatement(Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::CompoundStmtScope);
}

Statement::StmtPtr Parser::ParseCompoundStatement(Semantic::ScopeFlags flags)
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

Statement::StmtPtr Parser::ParseIfStatement()
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

Statement::StmtPtr Parser::ParseWhileStatement()
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

Statement::StmtPtr Parser::ParseForStatement()
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

	return m_Sema.ActOnForStmt(forLoc, leftParenLoc, std::move(initPart), std::move(condPart), std::move(thirdPart),
							   rightParenLoc, std::move(body));
}

Statement::StmtPtr Parser::ParseContinueStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_continue));
	const auto loc = m_CurrentToken.GetLocation();
	ConsumeToken();
	if (m_CurrentToken.Is(TokenType::Semi))
	{
		ConsumeToken();
	}
	else
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			  .AddArgument(TokenType::Semi)
			  .AddArgument(m_CurrentToken.GetType());
	}
	return m_Sema.ActOnContinueStmt(loc, m_Sema.GetCurrentScope());
}

Statement::StmtPtr Parser::ParseBreakStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_break));
	const auto loc = m_CurrentToken.GetLocation();
	ConsumeToken();
	if (m_CurrentToken.Is(TokenType::Semi))
	{
		ConsumeToken();
	}
	else
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			  .AddArgument(TokenType::Semi)
			  .AddArgument(m_CurrentToken.GetType());
	}
	return m_Sema.ActOnBreakStmt(loc, m_Sema.GetCurrentScope());
}

// return-statement:
//	'return' [expression] [;]
Statement::StmtPtr Parser::ParseReturnStatement()
{
	assert(m_CurrentToken.Is(TokenType::Kw_return));
	const auto loc = m_CurrentToken.GetLocation();
	ConsumeToken();

	Expression::ExprPtr returnedExpr;

	if (const auto funcDecl = m_Sema.GetParsingFunction())
	{
		const auto funcType = funcDecl->GetValueType().Cast<Type::FunctionType>();
		assert(funcType);
		const auto retType = funcType->GetResultType();
		if ((!retType && !m_CurrentToken.Is(TokenType::Semi)) || !retType->IsVoid())
		{
			returnedExpr = ParseExpression();
			if (m_CurrentToken.Is(TokenType::Semi))
			{
				ConsumeToken();
			}
			else
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
					  .AddArgument(TokenType::Semi)
					  .AddArgument(m_CurrentToken.GetType());
			}
		}

		return m_Sema.ActOnReturnStmt(loc, std::move(returnedExpr), m_Sema.GetCurrentScope());
	}

	// TODO: 报告错误：仅能在函数中返回
	return nullptr;
}

Statement::StmtPtr Parser::ParseExprStatement(nBool mayBeExpr)
{
	auto expr = ParseExpression();

	if (m_CurrentToken.Is(TokenType::Semi))
	{
		ConsumeToken();
		return m_Sema.ActOnExprStmt(std::move(expr));
	}

	if (!mayBeExpr)
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::Semi)
			.AddArgument(m_CurrentToken.GetType());
	}

	return expr;
}

Expression::ExprPtr Parser::ParseExpression()
{
	return ParseRightOperandOfBinaryExpression(ParseAssignmentExpression());
}

Expression::ExprPtr Parser::ParseUnaryExpression()
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
		result = m_Sema.ActOnIdExpr(m_Sema.GetCurrentScope(), nullptr, std::move(id),
									m_CurrentToken.Is(TokenType::LeftParen), m_ResolveContext);
		ConsumeToken();

		break;
	}
	case TokenType::PlusPlus:
	case TokenType::MinusMinus:
	case TokenType::Star:
	case TokenType::Amp:
	case TokenType::Plus:
	case TokenType::Minus:
	case TokenType::Exclaim:
	case TokenType::Tilde:
	{
		const auto loc = m_CurrentToken.GetLocation();
		ConsumeToken();
		result = ParseUnaryExpression();
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
		// TODO: 分清 Context
		ParseCompilerAction(Declaration::Context::Block, [&result](natRefPointer<ASTNode> const& node)
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

	return ParsePostfixExpressionSuffix(std::move(result));
}

Expression::ExprPtr Parser::ParseRightOperandOfBinaryExpression(Expression::ExprPtr leftOperand,
																OperatorPrecedence minPrec)
{
	auto tokenPrec = GetOperatorPrecedence(m_CurrentToken.GetType());
	SourceLocation colonLoc;

	while (true)
	{
		if (tokenPrec < minPrec)
		{
			return leftOperand;
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

		auto rightOperand = tokenPrec <= OperatorPrecedence::Conditional
								? ParseAssignmentExpression()
								: ParseUnaryExpression();
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
															   static_cast<OperatorPrecedence>(static_cast<std::underlying_type_t
																   <OperatorPrecedence>>(prevPrec) + !isRightAssoc));
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
			leftOperand = ternaryMiddle
							  ? m_Sema.ActOnConditionalOp(opToken.GetLocation(), colonLoc, std::move(leftOperand),
														  std::move(ternaryMiddle), std::move(rightOperand))
							  : m_Sema.ActOnBinaryOp(m_Sema.GetCurrentScope(), opToken.GetLocation(), opToken.GetType(),
													 std::move(leftOperand), std::move(rightOperand));
		}
	}
}

Expression::ExprPtr Parser::ParsePostfixExpressionSuffix(Expression::ExprPtr prefix)
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

				prefix = ParseExprError();
				break;
			}

			const auto rloc = m_CurrentToken.GetLocation();
			prefix = m_Sema.ActOnArraySubscriptExpr(m_Sema.GetCurrentScope(), std::move(prefix), lloc, std::move(index), rloc);
			ConsumeBracket();

			break;
		}
		case TokenType::LeftParen:
		{
			std::vector<Expression::ExprPtr> argExprs;
			std::vector<SourceLocation> commaLocs;

			const auto lloc = m_CurrentToken.GetLocation();
			ConsumeParen();

			if (!m_CurrentToken.Is(TokenType::RightParen) && !ParseExpressionList(argExprs, commaLocs, TokenType::RightParen))
			{
				// TODO: 报告错误
				prefix = ParseExprError();
				break;
			}

			if (!m_CurrentToken.Is(TokenType::RightParen))
			{
				// TODO: 报告错误
				prefix = ParseExprError();
				break;
			}

			ConsumeParen();

			prefix = m_Sema.ActOnCallExpr(m_Sema.GetCurrentScope(), std::move(prefix), lloc, from(argExprs),
										  m_CurrentToken.GetLocation());

			break;
		}
		case TokenType::Period:
		{
			const auto periodLoc = m_CurrentToken.GetLocation();
			ConsumeToken();
			Identifier::IdPtr unqualifiedId;
			if (!ParseUnqualifiedId(unqualifiedId))
			{
				prefix = ParseExprError();
				break;
			}

			prefix = m_Sema.ActOnMemberAccessExpr(m_Sema.GetCurrentScope(), std::move(prefix), periodLoc, nullptr,
												  unqualifiedId);

			break;
		}
		case TokenType::PlusPlus:
		case TokenType::MinusMinus:
			prefix = m_Sema.ActOnPostfixUnaryOp(m_Sema.GetCurrentScope(), m_CurrentToken.GetLocation(), m_CurrentToken.GetType(),
												std::move(prefix));
			ConsumeToken();
			break;
		case TokenType::Kw_as:
		{
			const auto asLoc = m_CurrentToken.GetLocation();

			// 吃掉 as
			ConsumeToken();

			const auto decl = make_ref<Declaration::Declarator>(Declaration::Context::TypeName);
			ParseDeclarator(decl);
			if (!decl->IsValid())
			{
				// TODO: 报告错误
				prefix = ParseExprError();
				break;
			}

			auto type = decl->GetType();
			if (!type)
			{
				// TODO: 报告错误
				prefix = ParseExprError();
				break;
			}

			prefix = m_Sema.ActOnAsTypeExpr(m_Sema.GetCurrentScope(), std::move(prefix), std::move(type), asLoc);

			break;
		}
		default:
			return prefix;
		}
	}
}

Expression::ExprPtr Parser::ParseConstantExpression()
{
	return ParseRightOperandOfBinaryExpression(ParseUnaryExpression(), OperatorPrecedence::Conditional);
}

Expression::ExprPtr Parser::ParseAssignmentExpression()
{
	if (m_CurrentToken.Is(TokenType::Kw_throw))
	{
		return ParseThrowExpression();
	}

	return ParseRightOperandOfBinaryExpression(ParseUnaryExpression());
}

Expression::ExprPtr Parser::ParseThrowExpression()
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

Expression::ExprPtr Parser::ParseParenExpression()
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

nBool Parser::ParseExpressionList(std::vector<Expression::ExprPtr>& exprs, std::vector<SourceLocation>& commaLocs,
								  Lex::TokenType endToken)
{
	while (true)
	{
		auto expr = ParseAssignmentExpression();
		if (!expr)
		{
			SkipUntil({ endToken }, true);
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

	if (endToken != TokenType::Eof && !m_CurrentToken.Is(endToken))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			  .AddArgument(endToken)
			  .AddArgument(m_CurrentToken.GetType());
		return false;
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
		else if (m_CurrentToken.IsAnyOf({ TokenType::Tilde, TokenType::Kw_this }))
		{
			if (m_CurrentToken.Is(TokenType::Tilde))
			{
				ConsumeToken();
				if (!m_CurrentToken.Is(TokenType::Kw_this))
				{
					m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
						  .AddArgument(TokenType::Kw_this)
						  .AddArgument(m_CurrentToken.GetType());
					return;
				}

				decl->SetDestructor();
			}
			else
			{
				decl->SetConstructor();
			}

			ConsumeToken();
		}
		else if (context != Declaration::Context::Prototype && context != Declaration::Context::TypeName)
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
			return;
		}
	}

	// TODO: 验证其他上下文下是否需要延迟分析
	if (m_Sema.GetCurrentPhase() == Semantic::Sema::Phase::Phase1 && context != Declaration::Context::Prototype && context
		!= Declaration::Context::TypeName)
	{
		skipTypeAndInitializer(decl);
	}
	else
	{
		// 对于有 unsafe 说明符的声明符，允许在类型和初始化器中使用不安全的功能
		const auto curScope = m_Sema.GetCurrentScope();
		const auto flags = curScope->GetFlags();
		if (decl->GetSafety() == Specifier::Safety::Unsafe)
		{
			curScope->AddFlags(Semantic::ScopeFlags::UnsafeScope);
		}

		const auto scope = make_scope([curScope, flags]
		{
			curScope->SetFlags(flags);
		});

		// (: int)也可以？
		if (m_CurrentToken.Is(TokenType::Colon) || ((context == Declaration::Context::Prototype || context == Declaration::
			Context::TypeName) && !decl->GetIdentifier()))
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
//	specifier
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
		case TokenType::Kw_unsafe:
			if (decl->GetSafety() != Specifier::Safety::None)
			{
				// TODO: 报告错误：多个安全说明符
			}
			decl->SetSafety(Specifier::Safety::Unsafe);
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
		// 用户自定义类型
		// TODO: 处理 module
		decl->SetType(m_Sema.LookupTypeName(m_CurrentToken.GetIdentifierInfo(), m_CurrentToken.GetLocation(),
											m_Sema.GetCurrentScope(), nullptr));
		ConsumeToken();
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
		// TODO: 分清 Context
		ParseCompilerAction(Declaration::Context::TypeName, [&decl](natRefPointer<ASTNode> const& node)
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

	if (auto type = decl->GetType(); type && type->GetType() == Type::Type::Class)
	{
		natRefPointer<NestedNameSpecifier> nns;
		while (m_CurrentToken.Is(TokenType::Period))
		{
			ConsumeToken();

			if (!m_CurrentToken.Is(TokenType::Identifier))
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedTypeSpecifierGot, m_CurrentToken.GetLocation())
					  .AddArgument(tokenType);
				break;
			}

			nns = NestedNameSpecifier::Create(m_Sema.GetASTContext(), std::move(nns), std::move(type));

			type = m_Sema.LookupTypeName(m_CurrentToken.GetIdentifierInfo(), m_CurrentToken.GetLocation(),
										 m_Sema.GetCurrentScope(), nns);
			if (!type)
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedTypeSpecifierGot, m_CurrentToken.GetLocation())
					  .AddArgument(m_CurrentToken.GetIdentifierInfo());
				return;
			}

			ConsumeToken();
		}

		decl->SetType(std::move(type));
	}

	// 数组的指针和指针的数组和指针的数组的指针
	ParseArrayOrPointerType(decl);
}

void Parser::ParseTypeOfType(Declaration::DeclaratorPtr const& decl)
{
	assert(m_CurrentToken.Is(TokenType::Kw_typeof));
	ConsumeToken();
	if (!m_CurrentToken.Is(TokenType::LeftParen))
	{
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			  .AddArgument(TokenType::LeftParen)
			  .AddArgument(m_CurrentToken.GetType());
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

	ParseScope prototypeScope{
		this,
		Semantic::ScopeFlags::FunctionDeclarationScope | Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::
		FunctionPrototypeScope
	};

	if (!m_CurrentToken.Is(TokenType::RightParen))
	{
		if (decl->IsDestructor())
		{
			// TODO: 报告错误：析构函数不可以具有参数
			// ↑那为什么要写这个括号呢？【x
			return;
		}

		while (true)
		{
			auto param = make_ref<Declaration::Declarator>(Declaration::Context::Prototype);
			ParseDeclarator(param);
			if ((mayBeParenType && param->GetIdentifier()) || !param->GetType())
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

	Type::TypePtr retType;
	// 构造和析构函数没有返回类型
	if (decl->IsConstructor() || decl->IsDestructor())
	{
		retType = m_Sema.GetASTContext().GetBuiltinType(Type::BuiltinType::Void);
	}
	else
	{
		// 如果不是->且只有一个无名称参数，并且不是构造或者析构函数说明是普通的括号类型
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

		const auto retTypeDeclarator = make_ref<Declaration::Declarator>(Declaration::Context::Prototype);
		ParseType(retTypeDeclarator);

		retType = retTypeDeclarator->GetType();
	}

	decl->SetType(m_Sema.BuildFunctionType(retType, from(paramDecls)
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

void Parser::ParseArrayOrPointerType(Declaration::DeclaratorPtr const& decl)
{
	while (true)
	{
		switch (m_CurrentToken.GetType())
		{
		case TokenType::LeftSquare:
		{
			ConsumeBracket();

			const auto sizeExpr = ParseConstantExpression();
			nuLong result;
			if (!sizeExpr->EvaluateAsInt(result, m_Sema.GetASTContext()))
			{
				// TODO: 报告错误
			}

			decl->SetType(m_Sema.ActOnArrayType(decl->GetType(), static_cast<std::size_t>(result)));

			if (!m_CurrentToken.Is(TokenType::RightSquare))
			{
				m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
					  .AddArgument(TokenType::RightSquare)
					  .AddArgument(m_CurrentToken.GetType());
			}
			ConsumeAnyToken();
			break;
		}
		case TokenType::Star:
			ConsumeToken();
			decl->SetType(m_Sema.ActOnPointerType(m_Sema.GetCurrentScope(), decl->GetType()));
			break;
		default:
			return;
		}
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
		if (m_CurrentToken.Is(TokenType::LeftBrace))
		{
			const auto leftBraceLoc = m_CurrentToken.GetLocation();
			ConsumeBrace();

			if (!decl->GetType())
			{
				// TODO: 报告错误：此形式要求指定类型
				return;
			}

			std::vector<Expression::ExprPtr> argExprs;
			std::vector<SourceLocation> commaLocs;

			if (!ParseExpressionList(argExprs, commaLocs, TokenType::RightBrace))
			{
				// 应该已经报告了错误，仅返回即可
				return;
			}

			// 如果到达此处说明当前 Token 是右大括号
			const auto rightBraceLoc = m_CurrentToken.GetLocation();
			ConsumeBrace();

			decl->SetInitializer(m_Sema.ActOnInitExpr(decl->GetType(), leftBraceLoc, std::move(argExprs), rightBraceLoc));
		}
		else
		{
			decl->SetInitializer(ParseExpression());
		}

		if (m_CurrentToken.Is(TokenType::Semi))
		{
			ConsumeToken();
		}
		else
		{
			m_Diag.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				.AddArgument(TokenType::Semi)
				.AddArgument(m_CurrentToken.GetType());
		}

		return;
	}

	if (m_CurrentToken.Is(TokenType::LeftBrace))
	{
		if (decl->GetType()->GetType() != Type::Type::Function)
		{
			// TODO: 报告错误
			return;
		}

		// TODO: 考虑分离声明和定义的分析，或者在声明分析过时跳过再次对声明生成新的定义，这样可以允许环形引用函数
		ParseScope bodyScope{
			this,
			Semantic::ScopeFlags::FunctionScope | Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::CompoundStmtScope
		};
		auto funcDecl = m_Sema.ActOnStartOfFunctionDef(m_Sema.GetCurrentScope(), decl);
		decl->SetDecl(ParseFunctionBody(std::move(funcDecl), bodyScope));
	}

	// 不是 initializer，返回
}

nBool Parser::SkipUntil(std::initializer_list<Lex::TokenType> list, nBool dontConsume,
						std::vector<Token>* skippedTokens)
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
		case TokenType::RightParen: // 可能的不匹配括号，下同
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

	SkipUntil({ TokenType::Equal, TokenType::LeftBrace, TokenType::Semi }, true, &cachedTokens);

	switch (m_CurrentToken.GetType())
	{
	case TokenType::Equal:
		skipToken(&cachedTokens);
		SkipUntil({ TokenType::Semi }, false, &cachedTokens);
		break;
	case TokenType::LeftBrace:
		skipToken(&cachedTokens);
		SkipUntil({ TokenType::RightBrace }, false, &cachedTokens);
		break;
	case TokenType::Semi:
		skipToken(&cachedTokens);
		break;
	default:
		m_Diag.Report(DiagnosticsEngine::DiagID::ErrUnexpect, m_CurrentToken.GetLocation())
			  .AddArgument(m_CurrentToken.GetType());
		break;
	}

	decl->SetCachedTokens(move(cachedTokens));
}

Declaration::DeclPtr Parser::ResolveDeclarator(Declaration::DeclaratorPtr decl)
{
	assert(m_Sema.GetCurrentPhase() == Semantic::Sema::Phase::Phase2 && m_ResolveContext);
	assert(!decl->GetType() && !decl->GetInitializer());

	const auto oldUnresolvedDecl = decl->GetDecl();
	assert(oldUnresolvedDecl);

	pushCachedTokens(decl->GetAndClearCachedTokens());
	const auto tokensScope = make_scope([this]
	{
		popCachedTokens();
	});

	m_ResolveContext->StartResolvingDeclarator(decl);
	const auto resolveContextScope = make_scope([this, decl]
	{
		m_ResolveContext->EndResolvingDeclarator(decl);
	});

	const auto recoveryScope = make_scope(
		[this, curScope = m_Sema.GetCurrentScope(), curDeclContext = m_Sema.GetDeclContext()]
		{
			m_Sema.SetDeclContext(curDeclContext);
			m_Sema.SetCurrentScope(curScope);
		});
	m_Sema.SetCurrentScope(decl->GetDeclarationScope());
	m_Sema.SetDeclContext(decl->GetDeclarationContext());

	ParseDeclarator(decl, true);
	return m_Sema.HandleDeclarator(m_Sema.GetCurrentScope(), std::move(decl), oldUnresolvedDecl);
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
