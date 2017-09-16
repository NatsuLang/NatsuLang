#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;

Interpreter::InterpreterDiagIdMap::InterpreterDiagIdMap(natRefPointer<TextReader<StringType::Utf8>> const& reader)
{
	using DiagIDUnderlyingType = std::underlying_type_t<Diag::DiagnosticsEngine::DiagID>;

	std::unordered_map<nStrView, Diag::DiagnosticsEngine::DiagID> idNameMap;
	for (auto id = DiagIDUnderlyingType{}; id < static_cast<DiagIDUnderlyingType>(Diag::DiagnosticsEngine::DiagID::EndOfDiagID); ++id)
	{
		if (auto idName = Diag::DiagnosticsEngine::GetDiagIDName(static_cast<Diag::DiagnosticsEngine::DiagID>(id)))
		{
			idNameMap.emplace(idName, static_cast<Diag::DiagnosticsEngine::DiagID>(id));
		}
	}

	nString diagIDName;
	while (true)
	{
		auto curLine = reader->ReadLine();

		if (diagIDName.IsEmpty())
		{
			if (curLine.IsEmpty())
			{
				break;
			}

			diagIDName = std::move(curLine);
		}
		else
		{
			const auto iter = idNameMap.find(diagIDName);
			if (iter == idNameMap.cend())
			{
				// TODO: 报告错误的 ID
				break;
			}

			if (!m_IdMap.try_emplace(iter->second, std::move(curLine)).second)
			{
				// TODO: 报告重复的 ID
			}

			diagIDName.Clear();
		}
	}
}

Interpreter::InterpreterDiagIdMap::~InterpreterDiagIdMap()
{
}

nString Interpreter::InterpreterDiagIdMap::GetText(Diag::DiagnosticsEngine::DiagID id)
{
	const auto iter = m_IdMap.find(id);
	return iter == m_IdMap.cend() ? "(No available text)" : iter->second;
}

Interpreter::InterpreterDiagConsumer::InterpreterDiagConsumer(Interpreter& interpreter)
	: m_Interpreter{ interpreter }, m_Errored{ false }
{
}

Interpreter::InterpreterDiagConsumer::~InterpreterDiagConsumer()
{
}

void Interpreter::InterpreterDiagConsumer::HandleDiagnostic(Diag::DiagnosticsEngine::Level level,
	Diag::DiagnosticsEngine::Diagnostic const& diag)
{
	nuInt levelId;

	switch (level)
	{
	case Diag::DiagnosticsEngine::Level::Ignored:
	case Diag::DiagnosticsEngine::Level::Note:
	case Diag::DiagnosticsEngine::Level::Remark:
		levelId = natLog::Msg;
		break;
	case Diag::DiagnosticsEngine::Level::Warning:
		levelId = natLog::Warn;
		break;
	case Diag::DiagnosticsEngine::Level::Error:
	case Diag::DiagnosticsEngine::Level::Fatal:
	default:
		levelId = natLog::Err;
		break;
	}

	m_Interpreter.m_Logger.Log(levelId, diag.GetDiagMessage());

	const auto loc = diag.GetSourceLocation();
	if (loc.GetFileID())
	{
		auto [succeed, fileContent] = m_Interpreter.m_SourceManager.GetFileContent(loc.GetFileID());
		if (const auto line = loc.GetLineInfo(); succeed && line)
		{
			size_t offset{};
			for (nuInt i = 1; i < line; ++i)
			{
				offset = fileContent.Find(Environment::GetNewLine(), static_cast<ptrdiff_t>(offset));
				if (offset == nStrView::npos)
				{
					// TODO: 无法定位到源文件
					return;
				}

				offset += Environment::GetNewLine().GetSize();
			}

			const auto nextNewLine = fileContent.Find(Environment::GetNewLine(), static_cast<ptrdiff_t>(offset));
			const auto column = loc.GetColumnInfo();
			offset += column ? column - 1 : 0;
			if (nextNewLine <= offset)
			{
				// TODO: 无法定位到源文件
				return;
			}

			m_Interpreter.m_Logger.Log(levelId, fileContent.Slice(static_cast<ptrdiff_t>(offset), nextNewLine == nStrView::npos ? -1 : static_cast<ptrdiff_t>(nextNewLine)));
			m_Interpreter.m_Logger.Log(levelId, "^");
		}
	}

	m_Errored = Diag::DiagnosticsEngine::IsUnrecoverableLevel(level);
}

Interpreter::InterpreterASTConsumer::InterpreterASTConsumer(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
}

Interpreter::InterpreterASTConsumer::~InterpreterASTConsumer()
{
}

void Interpreter::InterpreterASTConsumer::Initialize(ASTContext& /*context*/)
{
}

void Interpreter::InterpreterASTConsumer::HandleTranslationUnit(ASTContext& context)
{
	const auto iter = m_NamedDecls.find("Main");
	natRefPointer<Declaration::FunctionDecl> mainDecl;
	if (iter == m_NamedDecls.cend() || !((mainDecl = iter->second)))
	{
		nat_Throw(InterpreterException, u8"无法找到名为 Main 的函数");
	}

	if (mainDecl->Decl::GetType() != Declaration::Decl::Function)
	{
		nat_Throw(InterpreterException, u8"找到了名为 Main 的方法，但需要一个函数");
	}

	m_Interpreter.m_Visitor->Visit(mainDecl->GetBody());
}

nBool Interpreter::InterpreterASTConsumer::HandleTopLevelDecl(Linq<Valued<Declaration::DeclPtr>> const& decls)
{
	for (auto&& decl : decls)
	{
		if (auto namedDecl = static_cast<natRefPointer<Declaration::NamedDecl>>(decl))
		{
			m_NamedDecls.emplace(namedDecl->GetIdentifierInfo()->GetName(), std::move(namedDecl));
		}

		m_UnnamedDecls.emplace_back(std::move(decl));
	}

	return true;
}

Interpreter::InterpreterExprVisitor::InterpreterExprVisitor(Interpreter& interpreter)
	: m_Interpreter{ interpreter }, m_ShouldPrint{ false }
{
}

Interpreter::InterpreterExprVisitor::~InterpreterExprVisitor()
{
}

void Interpreter::InterpreterExprVisitor::Clear() noexcept
{
	m_LastVisitedExpr = nullptr;
}

void Interpreter::InterpreterExprVisitor::PrintExpr(natRefPointer<Expression::Expr> const& expr)
{
	Visit(expr);
	if (m_LastVisitedExpr)
	{
		m_ShouldPrint = true;
		Visit(m_LastVisitedExpr);
		m_ShouldPrint = false;
	}
}

Expression::ExprPtr Interpreter::InterpreterExprVisitor::GetLastVisitedExpr() const noexcept
{
	return m_LastVisitedExpr;
}

void Interpreter::InterpreterExprVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此表达式无法被访问");
}

void Interpreter::InterpreterExprVisitor::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	m_LastVisitedExpr = expr;
}

void Interpreter::InterpreterExprVisitor::VisitBooleanLiteral(natRefPointer<Expression::BooleanLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg("(表达式) {0}", expr->GetValue() ? "true" : "false");
	}
}

void Interpreter::InterpreterExprVisitor::VisitCharacterLiteral(natRefPointer<Expression::CharacterLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg("(表达式) '{0}'", U32StringView{ static_cast<U32StringView::CharType>(expr->GetCodePoint()) });
	}
}

void Interpreter::InterpreterExprVisitor::VisitDeclRefExpr(natRefPointer<Expression::DeclRefExpr> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		const auto decl = expr->GetDecl();
		const auto iter = m_Interpreter.m_DeclStorage.find(decl);
		if (iter == m_Interpreter.m_DeclStorage.cend())
		{
			nat_Throw(InterpreterException, u8"表达式引用了一个不存在的值定义");
		}

		visit([this, id = decl->GetIdentifierInfo()](auto value)
		{
			m_Interpreter.m_Logger.LogMsg("(声明 : {0}) {1}", id ? id->GetName() : "(临时对象)", value);
		}, iter->second);
	}
}

void Interpreter::InterpreterExprVisitor::VisitFloatingLiteral(natRefPointer<Expression::FloatingLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg("(表达式) {0}", expr->GetValue());
	}
}

void Interpreter::InterpreterExprVisitor::VisitIntegerLiteral(natRefPointer<Expression::IntegerLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg("(表达式) {0}", expr->GetValue());
	}
}

void Interpreter::InterpreterExprVisitor::VisitStringLiteral(natRefPointer<Expression::StringLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg("(表达式) \"{0}\"", expr->GetValue());
	}
}

void Interpreter::InterpreterExprVisitor::VisitArraySubscriptExpr(natRefPointer<Expression::ArraySubscriptExpr> const& expr)
{
	Visit(expr->GetLeftOperand());
	const auto baseOperand = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr);
	natRefPointer<Declaration::ValueDecl> baseDecl;
	natRefPointer<Type::ArrayType> baseType;
	if (baseOperand)
	{
		baseDecl = baseOperand->GetDecl();
		baseType = baseOperand->GetExprType();
	}

	if (!baseOperand || !baseDecl || !baseType)
	{
		nat_Throw(InterpreterException, u8"基础操作数无法被计算为有效的定义引用表达式");
	}
	
	Visit(expr->GetRightOperand());
	nuLong indexValue;
	if (const auto indexDeclOperand = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr))
	{
		const auto iter = m_Interpreter.m_DeclStorage.find(indexDeclOperand->GetDecl());
		if (iter == m_Interpreter.m_DeclStorage.cend())
		{
			nat_Throw(InterpreterException, u8"下标操作数引用了一个不存在的值定义");
		}

		indexValue = std::get<0>(iter->second);
	}
	else if (const auto indexLiteralOperand = static_cast<natRefPointer<Expression::IntegerLiteral>>(m_LastVisitedExpr))
	{
		indexValue = indexLiteralOperand->GetValue();
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitConstructExpr(natRefPointer<Expression::ConstructExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitDeleteExpr(natRefPointer<Expression::DeleteExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitNewExpr(natRefPointer<Expression::NewExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitThisExpr(natRefPointer<Expression::ThisExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitThrowExpr(natRefPointer<Expression::ThrowExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitCallExpr(natRefPointer<Expression::CallExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitMemberCallExpr(natRefPointer<Expression::MemberCallExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitCastExpr(natRefPointer<Expression::CastExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitAsTypeExpr(natRefPointer<Expression::AsTypeExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitImplicitCastExpr(natRefPointer<Expression::ImplicitCastExpr> const& expr)
{
	Visit(expr->GetOperand());

	// TODO
	auto castToType = static_cast<natRefPointer<Type::BuiltinType>>(expr->GetExprType());
	auto tempObjDef = make_ref<Declaration::ValueDecl>(Declaration::Decl::Var, m_Interpreter.m_CurrentScope->GetEntity(), SourceLocation{}, nullptr, castToType);
	auto declRefExpr = make_ref<Expression::DeclRefExpr>(nullptr, tempObjDef, SourceLocation{}, castToType);

	if (castToType)
	{
		if (castToType->IsIntegerType())
		{
			if (const auto declOperand = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr))
			{
				const auto iter = m_Interpreter.m_DeclStorage.find(declOperand->GetDecl());
				if (iter != m_Interpreter.m_DeclStorage.cend())
				{
					const auto declValue = iter->second;
					visit([this, &tempObjDef](auto value)
					{
						m_Interpreter.m_DeclStorage.emplace(
							std::piecewise_construct,
							std::forward_as_tuple(tempObjDef),
							std::forward_as_tuple(std::in_place_index<0>, static_cast<nuLong>(value)));
					}, declValue);
				}
			}
			else if (const auto intLiteralOperand = static_cast<natRefPointer<Expression::IntegerLiteral>>(m_LastVisitedExpr))
			{
				m_Interpreter.m_DeclStorage.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(tempObjDef),
					std::forward_as_tuple(std::in_place_index<0>, intLiteralOperand->GetValue()));
			}
			else if (const auto floatLiteralOperand = static_cast<natRefPointer<Expression::FloatingLiteral>>(m_LastVisitedExpr))
			{
				m_Interpreter.m_DeclStorage.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(tempObjDef),
					std::forward_as_tuple(std::in_place_index<0>, static_cast<nuLong>(floatLiteralOperand->GetValue())));
			}
		}
		else if (castToType->IsFloatingType())
		{
			if (const auto declOperand = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr))
			{
				const auto iter = m_Interpreter.m_DeclStorage.find(declOperand->GetDecl());
				if (iter != m_Interpreter.m_DeclStorage.cend())
				{
					const auto declValue = iter->second;
					visit([this, &tempObjDef](auto value)
					{
						m_Interpreter.m_DeclStorage.emplace(
							std::piecewise_construct,
							std::forward_as_tuple(tempObjDef),
							std::forward_as_tuple(std::in_place_index<1>, static_cast<nDouble>(value)));
					}, declValue);
				}
			}
			else if (const auto intLiteralOperand = static_cast<natRefPointer<Expression::IntegerLiteral>>(m_LastVisitedExpr))
			{
				m_Interpreter.m_DeclStorage.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(tempObjDef),
					std::forward_as_tuple(std::in_place_index<1>, static_cast<nDouble>(intLiteralOperand->GetValue())));
			}
			else if (const auto floatLiteralOperand = static_cast<natRefPointer<Expression::FloatingLiteral>>(m_LastVisitedExpr))
			{
				m_Interpreter.m_DeclStorage.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(tempObjDef),
					std::forward_as_tuple(std::in_place_index<1>, floatLiteralOperand->GetValue()));
			}
		}
	}
	
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitMemberExpr(natRefPointer<Expression::MemberExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitParenExpr(natRefPointer<Expression::ParenExpr> const& expr)
{
	m_LastVisitedExpr = expr->GetInnerExpr();
}

void Interpreter::InterpreterExprVisitor::VisitStmtExpr(natRefPointer<Expression::StmtExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitUnaryExprOrTypeTraitExpr(natRefPointer<Expression::UnaryExprOrTypeTraitExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitConditionalOperator(natRefPointer<Expression::ConditionalOperator> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitBinaryOperator(natRefPointer<Expression::BinaryOperator> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitCompoundAssignOperator(natRefPointer<Expression::CompoundAssignOperator> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitUnaryOperator(natRefPointer<Expression::UnaryOperator> const& expr)
{
	const auto opCode = expr->GetOpcode();
	Visit(expr->GetOperand());
	const auto declExpr = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr);

	switch (opCode)
	{
	case Expression::UnaryOperationType::PostInc:
		break;
	case Expression::UnaryOperationType::PostDec:
		break;
	case Expression::UnaryOperationType::PreInc:
		if (declExpr)
		{
			const auto iter = m_Interpreter.m_DeclStorage.find(declExpr->GetDecl());
			if (iter != m_Interpreter.m_DeclStorage.cend())
			{
				visit([this](auto& value)
				{
					++value;
				}, iter->second);
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::PreDec:
		if (declExpr)
		{
			const auto iter = m_Interpreter.m_DeclStorage.find(declExpr->GetDecl());
			if (iter != m_Interpreter.m_DeclStorage.cend())
			{
				visit([this](auto& value)
				{
					--value;
				}, iter->second);
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::Plus:
		return;
	case Expression::UnaryOperationType::Minus:
		break;
	case Expression::UnaryOperationType::Not:
		break;
	case Expression::UnaryOperationType::LNot:
		break;
	case Expression::UnaryOperationType::Invalid:
	default:
		break;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

Interpreter::InterpreterStmtVisitor::InterpreterStmtVisitor(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
}

Interpreter::InterpreterStmtVisitor::~InterpreterStmtVisitor()
{
}

void Interpreter::InterpreterStmtVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此语句无法被访问");
}

void Interpreter::InterpreterStmtVisitor::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	InterpreterExprVisitor visitor{ m_Interpreter };
	visitor.PrintExpr(expr);
}

void Interpreter::InterpreterStmtVisitor::VisitBreakStmt(natRefPointer<Statement::BreakStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitCatchStmt(natRefPointer<Statement::CatchStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitTryStmt(natRefPointer<Statement::TryStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitCompoundStmt(natRefPointer<Statement::CompoundStmt> const& stmt)
{
	for (auto&& item : stmt->GetChildrens())
	{
		Visit(item);
	}
}

void Interpreter::InterpreterStmtVisitor::VisitContinueStmt(natRefPointer<Statement::ContinueStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitDeclStmt(natRefPointer<Statement::DeclStmt> const& stmt)
{
	for (auto&& decl : stmt->GetDecls())
	{
		if (auto varDecl = static_cast<natRefPointer<Declaration::VarDecl>>(decl))
		{
			if (auto builtinType = static_cast<natRefPointer<Type::BuiltinType>>(varDecl->GetValueType()))
			{
				if (builtinType->IsIntegerType())
				{
					InterpreterExprVisitor visitor{ m_Interpreter };
					visitor.Visit(varDecl->GetInitializer());
					m_Interpreter.m_DeclStorage.emplace(
						std::piecewise_construct,
						std::forward_as_tuple(varDecl),
						std::forward_as_tuple(std::in_place_index<0>, static_cast<natRefPointer<Expression::IntegerLiteral>>(visitor.GetLastVisitedExpr()))
					);
				}
			}
		}

		
	}
}

void Interpreter::InterpreterStmtVisitor::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitForStmt(natRefPointer<Statement::ForStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitGotoStmt(natRefPointer<Statement::GotoStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitIfStmt(natRefPointer<Statement::IfStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitLabelStmt(natRefPointer<Statement::LabelStmt> const& stmt)
{
	Visit(stmt->GetSubStmt());
}

void Interpreter::InterpreterStmtVisitor::VisitNullStmt(natRefPointer<Statement::NullStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitReturnStmt(natRefPointer<Statement::ReturnStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitCaseStmt(natRefPointer<Statement::CaseStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitDefaultStmt(natRefPointer<Statement::DefaultStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitSwitchStmt(natRefPointer<Statement::SwitchStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitWhileStmt(natRefPointer<Statement::WhileStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

Interpreter::Interpreter(natRefPointer<TextReader<StringType::Utf8>> const& diagIdMapFile, natLog& logger)
	: m_DiagConsumer{ make_ref<InterpreterDiagConsumer>(*this) },
	m_Diag{ make_ref<InterpreterDiagIdMap>(diagIdMapFile), m_DiagConsumer },
	m_Logger{ logger },
	m_SourceManager{ m_Diag, m_FileManager },
	m_Preprocessor{ m_Diag, m_SourceManager },
	m_Consumer{ make_ref<InterpreterASTConsumer>(*this) },
	m_Sema{ m_Preprocessor, m_AstContext, m_Consumer },
	m_Parser{ m_Preprocessor, m_Sema },
	m_Visitor{ make_ref<InterpreterStmtVisitor>(*this) }
{
}

Interpreter::~Interpreter()
{
}

void Interpreter::Run(Uri const& uri)
{
	m_Preprocessor.SetLexer(make_ref<Lex::Lexer>(m_SourceManager.GetFileContent(m_SourceManager.GetFileID(uri)).second, m_Preprocessor));
	ParseAST(m_Parser);
}

void Interpreter::Run(nStrView content)
{
	m_Preprocessor.SetLexer(make_ref<Lex::Lexer>(content, m_Preprocessor));
	m_Parser.ConsumeToken();
	const auto stmt = m_Parser.ParseStatement();
	if (!stmt || m_DiagConsumer->IsErrored())
	{
		m_DiagConsumer->Reset();
		nat_Throw(InterpreterException, "编译语句 \"{0}\" 失败", content);
	}

	m_Visitor->Visit(stmt);
}

natRefPointer<Semantic::Scope> Interpreter::GetScope() const noexcept
{
	return m_CurrentScope;
}
