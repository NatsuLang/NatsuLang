#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Detail;

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
		else
		{
			m_UnnamedDecls.emplace_back(std::move(decl));
		}
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
		if (!m_Interpreter.m_DeclStorage.DoesDeclExist(decl))
		{
			nat_Throw(InterpreterException, u8"表达式引用了一个不存在的值定义");
		}

		m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(decl), [this, id = decl->GetIdentifierInfo()](auto value)
		{
			m_Interpreter.m_Logger.LogMsg("(声明 : {0}) {1}", id ? id->GetName() : u8"(临时对象)"_nv, value);
		});
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
		auto decl = indexDeclOperand->GetDecl();
		if (!m_Interpreter.m_DeclStorage.DoesDeclExist(decl))
		{
			nat_Throw(InterpreterException, u8"下标操作数引用了一个不存在的值定义");
		}

		m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(decl), [&indexValue](auto value)
		{
			indexValue = value;
		}, Expected<nuLong>);
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
	auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(castToType);
	const auto declRefExpr = make_ref<Expression::DeclRefExpr>(nullptr, tempObjDef, SourceLocation{}, castToType);

	if (castToType)
	{
		if (castToType->IsIntegerType())
		{
			if (const auto declOperand = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr))
			{
				m_Interpreter.m_DeclStorage.VisitDeclStorage(declOperand->GetDecl(), [this, &tempObjDef](auto const& value)
				{
					m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [&value](auto& storage)
					{
						storage = static_cast<nuLong>(value);
					}, Expected<nuLong>);
				}, Expected<nuLong>);
			}
			else if (const auto intLiteralOperand = static_cast<natRefPointer<Expression::IntegerLiteral>>(m_LastVisitedExpr))
			{
				const auto value = intLiteralOperand->GetValue();
				m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& storage)
				{
					storage = value;
				}, Expected<nuLong>);
			}
			else if (const auto floatLiteralOperand = static_cast<natRefPointer<Expression::FloatingLiteral>>(m_LastVisitedExpr))
			{
				const auto value = static_cast<nuLong>(floatLiteralOperand->GetValue());
				m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& storage)
				{
					storage = value;
				}, Expected<nuLong>);
			}
		}
		else if (castToType->IsFloatingType())
		{
			if (const auto declOperand = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr))
			{
				m_Interpreter.m_DeclStorage.VisitDeclStorage(declOperand->GetDecl(), [this, &tempObjDef](auto const& value)
				{
					m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [&value](auto& storage)
					{
						storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
					}, Expected<nFloat, nDouble>);
				}, Expected<nFloat, nDouble>);
			}
			else if (const auto intLiteralOperand = static_cast<natRefPointer<Expression::IntegerLiteral>>(m_LastVisitedExpr))
			{
				const auto value = static_cast<nDouble>(intLiteralOperand->GetValue());
				m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& storage)
				{
					storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
				}, Expected<nFloat, nDouble>);
			}
			else if (const auto floatLiteralOperand = static_cast<natRefPointer<Expression::FloatingLiteral>>(m_LastVisitedExpr))
			{
				const auto value = floatLiteralOperand->GetValue();
				m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& storage)
				{
					storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
				}, Expected<nFloat, nDouble>);
			}
		}

		m_LastVisitedExpr = declRefExpr;
		return;
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

// TODO: 在规范中定义对象被销毁的时机
void Interpreter::InterpreterExprVisitor::VisitUnaryOperator(natRefPointer<Expression::UnaryOperator> const& expr)
{
	const auto opCode = expr->GetOpcode();
	Visit(expr->GetOperand());
	const auto declExpr = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr);

	switch (opCode)
	{
	case Expression::UnaryOperationType::PostInc:
		if (declExpr)
		{
			const auto decl = declExpr->GetDecl();
			if (!decl->GetIdentifierInfo())
			{
				nat_Throw(InterpreterException, u8"不允许修改临时对象");
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &decl](auto& value)
				{
					auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(decl->GetValueType());
					if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& tmpValue)
						{
							tmpValue = value;
						}, Expected<std::remove_reference_t<decltype(value)>>))
					{
						nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
					}

					m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, decl->GetValueType());
					++value;
				}, Excepted<nBool>))
			{
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::PostDec:
		if (declExpr)
		{
			const auto decl = declExpr->GetDecl();
			if (!decl->GetIdentifierInfo())
			{
				nat_Throw(InterpreterException, u8"不允许修改临时对象");
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &decl](auto& value)
			{
				auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(decl->GetValueType());
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& tmpValue)
				{
					tmpValue = value;
				}, Expected<std::remove_reference_t<decltype(value)>>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}

				m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, decl->GetValueType());
				--value;
			}, Excepted<nBool>))
			{
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::PreInc:
		if (declExpr)
		{
			const auto decl = declExpr->GetDecl();
			if (!decl->GetIdentifierInfo())
			{
				nat_Throw(InterpreterException, u8"不允许修改临时对象");
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(decl), [](auto& value)
				{
					++value;
				}, Excepted<nBool>))
			{
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::PreDec:
		if (declExpr)
		{
			const auto decl = declExpr->GetDecl();
			if (!decl->GetIdentifierInfo())
			{
				nat_Throw(InterpreterException, u8"不允许修改临时对象");
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(decl), [](auto& value)
				{
					--value;
				}, Excepted<nBool>))
			{
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::Plus:
		return;
	case Expression::UnaryOperationType::Minus:
		if (declExpr)
		{
			const auto decl = declExpr->GetDecl();
			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &decl](auto value)
			{
				auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(decl->GetValueType());
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& tmpValue)
				{
					tmpValue = decltype(value){} - value;
				}, Expected<decltype(value)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}

				m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, decl->GetValueType());
			}, Excepted<nBool>))
			{
				return;
			}
		}
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
		if (!decl)
		{
			nat_Throw(InterpreterException, u8"错误的声明");
		}

		if (auto varDecl = static_cast<natRefPointer<Declaration::VarDecl>>(decl))
		{
			// TODO: 修改为通用的实现
			InterpreterExprVisitor visitor{ m_Interpreter };
			const auto initializer = varDecl->GetInitializer();
			if (!initializer)
			{
				continue;
			}

			visitor.Visit(initializer);
			const auto initExpr = visitor.GetLastVisitedExpr();

			if (auto declExpr = static_cast<natRefPointer<Expression::DeclRefExpr>>(initExpr))
			{
				auto succeed = false;
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(varDecl, [this, &succeed, declExpr = std::move(declExpr)](auto& storage)
				{
					succeed = m_Interpreter.m_DeclStorage.VisitDeclStorage(declExpr->GetDecl(), [&storage](auto& opStorage)
					{
						storage = opStorage;
					}, Expected<std::remove_reference_t<decltype(storage)>>);
				}) || !succeed)
				{
					nat_Throw(InterpreterException, u8"无法访问存储");
				}

				continue;
			}
			
			if (const auto builtinType = static_cast<natRefPointer<Type::BuiltinType>>(initExpr->GetExprType()))
			{
				if (const auto intLiteral = static_cast<natRefPointer<Expression::IntegerLiteral>>(initExpr))
				{
					const auto value = intLiteral->GetValue();
					if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(varDecl, [value](auto& storage)
				                                                  {
					                                                  storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
				                                                  }, Expected<nShort, nuShort, nInt, nuInt, nLong, nuLong>))
					{
						nat_Throw(InterpreterException, u8"无法访问存储");
					}

					continue;
				}
				
				if (const auto floatingLiteral = static_cast<natRefPointer<Expression::FloatingLiteral>>(initExpr))
				{
					const auto value = floatingLiteral->GetValue();
					if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(varDecl, [value](auto& storage)
				                                                  {
					                                                  storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
				                                                  }, Expected<nFloat, nDouble>))
					{
						nat_Throw(InterpreterException, u8"无法访问存储");
					}

					continue;
				}
				
				if (const auto boolLiteral = static_cast<natRefPointer<Expression::BooleanLiteral>>(initExpr))
				{
					const auto value = boolLiteral->GetValue();
					if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(varDecl, [value](auto& storage)
				                                                  {
					                                                  storage = value;
				                                                  }, Expected<nBool>))
					{
						nat_Throw(InterpreterException, u8"无法访问存储");
					}

					continue;
				}
			}

			nat_Throw(InterpreterException, u8"此功能尚未实现");
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

void Interpreter::InterpreterDeclStorage::StorageDeleter::operator()(nData data) const noexcept
{
#ifdef _MSC_VER
#	ifdef NDEBUG
		_aligned_free(data);
#	else
		_aligned_free_dbg(data);
#	endif
#else
	std::free(data);
#endif
}

Interpreter::InterpreterDeclStorage::InterpreterDeclStorage(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
}

std::pair<nBool, nData> Interpreter::InterpreterDeclStorage::GetOrAddDecl(natRefPointer<Declaration::ValueDecl> decl)
{
	const auto type = decl->GetValueType();
	assert(type);
	const auto typeInfo = m_Interpreter.m_AstContext.GetTypeInfo(type);

	auto iter = m_DeclStorage.find(decl);
	if (iter != m_DeclStorage.cend())
	{
		return { false, iter->second.get() };
	}

	const auto storagePointer =
#ifdef _MSC_VER
#	ifdef NDEBUG
		static_cast<nData>(_aligned_malloc(typeInfo.Size, typeInfo.Align));
#	else
		static_cast<nData>(_aligned_malloc_dbg(typeInfo.Size, typeInfo.Align, __FILE__, __LINE__));
#	endif
#else
		static_cast<nData>(std::aligned_alloc(typeInfo.Align, typeInfo.Size));
#endif

	if (storagePointer)
	{
		nBool succeed;
		tie(iter, succeed) = m_DeclStorage.emplace(std::move(decl), std::unique_ptr<nByte[], StorageDeleter>{ storagePointer });

		if (succeed)
		{
			std::memset(iter->second.get(), 0, typeInfo.Size);
			return { true, iter->second.get() };
		}
	}

	nat_Throw(InterpreterException, "无法为此声明创建存储");
}

void Interpreter::InterpreterDeclStorage::RemoveDecl(natRefPointer<Declaration::ValueDecl> const& decl)
{
	// TODO: 是否需要将声明从所在的域中移除？
	m_DeclStorage.erase(decl);
}

nBool Interpreter::InterpreterDeclStorage::DoesDeclExist(natRefPointer<Declaration::ValueDecl> const& decl) const noexcept
{
	return m_DeclStorage.find(decl) != m_DeclStorage.cend();
}

void Interpreter::InterpreterDeclStorage::GarbageCollect()
{
	for (auto iter = m_DeclStorage.begin(); iter != m_DeclStorage.end();)
	{
		// 没有外部引用了，回收这个声明及占用的存储
		if (iter->first->IsUnique())
		{
			iter = m_DeclStorage.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

natRefPointer<Declaration::ValueDecl> Interpreter::InterpreterDeclStorage::CreateTemporaryObjectDecl(Type::TypePtr type, SourceLocation loc)
{
	return make_ref<Declaration::ValueDecl>(Declaration::Decl::Var,	nullptr, loc, nullptr, type);
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
	  m_Visitor{ make_ref<InterpreterStmtVisitor>(*this) }, m_DeclStorage{ *this }
{
}

Interpreter::~Interpreter()
{
}

void Interpreter::Run(Uri const& uri)
{
	m_Preprocessor.SetLexer(make_ref<Lex::Lexer>(m_SourceManager.GetFileContent(m_SourceManager.GetFileID(uri)).second, m_Preprocessor));
	m_Parser.ConsumeToken();
	m_CurrentScope = m_Sema.GetCurrentScope();
	ParseAST(m_Parser);
	m_DeclStorage.GarbageCollect();
}

void Interpreter::Run(nStrView content)
{
	m_Preprocessor.SetLexer(make_ref<Lex::Lexer>(content, m_Preprocessor));
	m_Parser.ConsumeToken();
	m_CurrentScope = m_Sema.GetCurrentScope();
	const auto stmt = m_Parser.ParseStatement();
	if (!stmt || m_DiagConsumer->IsErrored())
	{
		m_DiagConsumer->Reset();
		nat_Throw(InterpreterException, "编译语句 \"{0}\" 失败", content);
	}

	m_Visitor->Visit(stmt);
	m_DeclStorage.GarbageCollect();
}

natRefPointer<Semantic::Scope> Interpreter::GetScope() const noexcept
{
	return m_CurrentScope;
}
