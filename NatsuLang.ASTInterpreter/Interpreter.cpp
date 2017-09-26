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

	m_Errored |= Diag::DiagnosticsEngine::IsUnrecoverableLevel(level);
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
	Visit(expr->GetCallee());
	const auto callee = static_cast<natRefPointer<Expression::DeclRefExpr>>(m_LastVisitedExpr);
	if (!callee)
	{
		nat_Throw(InterpreterException, u8"被调用者错误");
	}

	if (const auto calleeDecl = static_cast<natRefPointer<Declaration::FunctionDecl>>(callee->GetDecl()))
	{
		m_Interpreter.m_Sema.PushScope(Semantic::ScopeFlags::BlockScope |
			Semantic::ScopeFlags::CompoundStmtScope |
			Semantic::ScopeFlags::DeclarableScope |
			Semantic::ScopeFlags::FunctionScope);
		m_Interpreter.m_Sema.PushDeclContext(m_Interpreter.m_Sema.GetCurrentScope(), calleeDecl.Get());
		m_Interpreter.m_CurrentScope = m_Interpreter.m_Sema.GetCurrentScope();

		const auto scope = make_scope([this]
		{
			m_Interpreter.m_Sema.PopDeclContext();
			m_Interpreter.m_Sema.PopScope();
			m_Interpreter.m_CurrentScope = m_Interpreter.m_Sema.GetCurrentScope();
		});

		// TODO: 允许默认参数
		assert(expr->GetArgCount() == calleeDecl->GetParamCount());

		for (auto&& param : calleeDecl->GetParams().zip(expr->GetArgs()))
		{
			if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(param.first, [this, &param](auto& storage)
			{
				if (!Evaluate(param.second, [&storage](auto value)
				{
					storage = value;
				}, Expected<std::remove_reference_t<decltype(storage)>>))
				{
					nat_Throw(InterpreterException, u8"无法对操作数求值");
				}
			}))
			{
				nat_Throw(InterpreterException, u8"无法为参数的定义分配存储");
			}
		}

		InterpreterStmtVisitor stmtVisitor{ m_Interpreter };
		stmtVisitor.Visit(calleeDecl->GetBody());
		m_LastVisitedExpr = stmtVisitor.GetReturnedExpr();
		if (!m_LastVisitedExpr)
		{
			const auto retType = static_cast<natRefPointer<Type::BuiltinType>>(static_cast<natRefPointer<Type::FunctionType>>(calleeDecl->GetValueType())->GetResultType());
			if (!retType || retType->GetBuiltinClass() != Type::BuiltinType::Void)
			{
				nat_Throw(InterpreterException, u8"要求返回值的函数在控制流离开后未返回任何值");
			}
		}

		return;
	}

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
	Visit(expr->GetOperand());

	auto castToType = static_cast<natRefPointer<Type::BuiltinType>>(expr->GetExprType());
	auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(castToType);
	const auto declRefExpr = make_ref<Expression::DeclRefExpr>(nullptr, tempObjDef, SourceLocation{}, castToType);

	if (Evaluate(m_LastVisitedExpr, [this, tempObjDef = std::move(tempObjDef)](auto value)
	{
		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(tempObjDef), [value](auto& storage)
		{
			storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
		}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
		{
			nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
		}
	}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
	{
		m_LastVisitedExpr = std::move(declRefExpr);
		return;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitImplicitCastExpr(natRefPointer<Expression::ImplicitCastExpr> const& expr)
{
	Visit(expr->GetOperand());

	auto castToType = static_cast<natRefPointer<Type::BuiltinType>>(expr->GetExprType());
	auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(castToType);
	const auto declRefExpr = make_ref<Expression::DeclRefExpr>(nullptr, tempObjDef, SourceLocation{}, castToType);

	if (Evaluate(m_LastVisitedExpr, [this, tempObjDef = std::move(tempObjDef)](auto value)
	{
		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(tempObjDef), [value](auto& storage)
		{
			storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
		}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
		{
			nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
		}
	}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
	{
		m_LastVisitedExpr = std::move(declRefExpr);
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
	Visit(expr->GetInnerExpr());
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
	const auto opCode = expr->GetOpcode();
	Visit(expr->GetLeftOperand());
	const auto leftOperand = std::move(m_LastVisitedExpr);
	Visit(expr->GetRightOperand());
	const auto rightOperand = std::move(m_LastVisitedExpr);

	auto tempObjDecl = InterpreterDeclStorage::CreateTemporaryObjectDecl(expr->GetExprType());

	auto evalSucceed = false;

	switch (opCode)
	{
	case Expression::BinaryOperationType::Mul:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue * rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Div:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!rightValue)
				{
					nat_Throw(InterpreterException, u8"被0除");
				}

				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue / rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Mod:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!rightValue)
				{
					nat_Throw(InterpreterException, u8"被0除");
				}

				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue % rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, nFloat, nDouble>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Add:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue + rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Sub:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue - rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Shl:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue << rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, nFloat, nDouble>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Shr:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue >> rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, nFloat, nDouble>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::LT:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](nBool& storage)
				{
					storage = leftValue < rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::GT:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue > rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::LE:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue <= rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::GE:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue >= rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::EQ:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue == rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::NE:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue != rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::And:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue & rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, nFloat, nDouble>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Xor:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue ^ rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, nFloat, nDouble>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::Or:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue | rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, nFloat, nDouble>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::LAnd:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](nBool& storage)
				{
					storage = leftValue && rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	case Expression::BinaryOperationType::LOr:
		if (Evaluate(leftOperand, [this, &evalSucceed, rightOperand = std::move(rightOperand), tempObjDecl](auto leftValue)
		{
			evalSucceed = Evaluate(rightOperand, [this, leftValue, tempObjDecl](auto rightValue)
			{
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](nBool& storage)
				{
					storage = leftValue || rightValue;
				}, Expected<nBool>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储");
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView>) && evalSucceed)
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDecl), SourceLocation{}, expr->GetExprType());
			return;
		}
		break;
	default:
		assert(!"Invalid opcode.");
		[[fallthrough]];
	case Expression::BinaryOperationType::Invalid:
		break;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterExprVisitor::VisitCompoundAssignOperator(natRefPointer<Expression::CompoundAssignOperator> const& expr)
{
	const auto opCode = expr->GetOpcode();
	Visit(expr->GetLeftOperand());
	const auto leftOperand = std::move(m_LastVisitedExpr);
	const auto rightOperand = expr->GetRightOperand();

	const auto leftDeclExpr = static_cast<natRefPointer<Expression::DeclRefExpr>>(leftOperand);

	natRefPointer<Declaration::ValueDecl> decl;
	if (!leftDeclExpr || !((decl = leftDeclExpr->GetDecl())) || !decl->GetIdentifierInfo())
	{
		nat_Throw(InterpreterException, u8"左操作数必须是对非临时对象的定义的引用");
	}

	nBool visitSucceed, evalSucceed;

	switch (opCode)
	{
	case Expression::BinaryOperationType::Assign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage = value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		});
		break;
	case Expression::BinaryOperationType::MulAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage *= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool>);
		break;
	case Expression::BinaryOperationType::DivAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage /= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool>);
		break;
	case Expression::BinaryOperationType::RemAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage %= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble>);
		break;
	case Expression::BinaryOperationType::AddAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage += value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool>);
		break;
	case Expression::BinaryOperationType::SubAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage -= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool>);
		break;
	case Expression::BinaryOperationType::ShlAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage <<= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble>);
		break;
	case Expression::BinaryOperationType::ShrAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage >>= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble>);
		break;
	case Expression::BinaryOperationType::AndAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage &= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble>);
		break;
	case Expression::BinaryOperationType::XorAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage ^= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble>);
		break;
	case Expression::BinaryOperationType::OrAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage |= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble>);
		break;
	case Expression::BinaryOperationType::Invalid:
	default:
		nat_Throw(InterpreterException, u8"错误的操作");
	}

	if (!visitSucceed || !evalSucceed)
	{
		nat_Throw(InterpreterException, u8"操作失败");
	}

	m_LastVisitedExpr = std::move(leftDeclExpr);
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
	{
		auto type = m_LastVisitedExpr->GetExprType();
		auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(type);
		if (m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [this, expr = std::move(m_LastVisitedExpr)](auto& tmpValue)
		{
			if (!Evaluate(expr, [&tmpValue](auto value)
			{
				tmpValue = decltype(value){} - value;
			}, Expected<std::remove_reference_t<decltype(tmpValue)>>))
			{
				nat_Throw(InterpreterException, u8"无法对操作数求值");
			}
		}, Excepted<nStrView, nBool>))
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, std::move(type));
			return;
		}

		break;
	}
	case Expression::UnaryOperationType::Not:
	{
		auto type = m_LastVisitedExpr->GetExprType();
		auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(type);
		if (m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [this, expr = std::move(m_LastVisitedExpr)](auto& tmpValue)
		{
			if (!Evaluate(expr, [&tmpValue](auto value)
			{
				tmpValue = ~value;
			}, Expected<std::remove_reference_t<decltype(tmpValue)>>))
			{
				nat_Throw(InterpreterException, u8"无法对操作数求值");
			}
		}, Excepted<nStrView, nBool, nFloat, nDouble>))
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, std::move(type));
			return;
		}

		break;
	}
	case Expression::UnaryOperationType::LNot:
	{
		auto boolType = m_Interpreter.m_AstContext.GetBuiltinType(Type::BuiltinType::Bool);
		auto type = m_LastVisitedExpr->GetExprType();
		auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(boolType);
		if (m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [this, expr = std::move(m_LastVisitedExpr)](auto& tmpValue)
		{
			if (!Evaluate(expr, [&tmpValue](auto value)
			{
				tmpValue = !value;
			}, Excepted<nStrView>))
			{
				nat_Throw(InterpreterException, u8"无法对操作数求值");
			}
		}, Expected<nBool>))
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, std::move(boolType));
			return;
		}

		break;
	}
	case Expression::UnaryOperationType::Invalid:
	default:
		break;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

Interpreter::InterpreterStmtVisitor::InterpreterStmtVisitor(Interpreter& interpreter)
	: m_Interpreter{ interpreter }, m_Returned{ false }
{
}

Interpreter::InterpreterStmtVisitor::~InterpreterStmtVisitor()
{
}

Expression::ExprPtr Interpreter::InterpreterStmtVisitor::GetReturnedExpr() const noexcept
{
	return m_ReturnedExpr;
}

void Interpreter::InterpreterStmtVisitor::ResetReturnedExpr() noexcept
{
	m_Returned = false;
	m_ReturnedExpr.Reset();
}

void Interpreter::InterpreterStmtVisitor::Visit(natRefPointer<Statement::Stmt> const& stmt)
{
	if (m_Returned || m_ReturnedExpr)
	{
		return;
	}

	StmtVisitor::Visit(stmt);
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
			// 不需要分配存储
			if (varDecl->IsFunction())
			{
				continue;
			}

			auto succeed = false;
			if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(varDecl, [this, initializer = varDecl->GetInitializer(), &succeed](auto& storage)
			{
				InterpreterExprVisitor visitor{ m_Interpreter };
				succeed = visitor.Evaluate(initializer, [&storage](auto value)
				{
					storage = value;
				}, Expected<std::remove_reference_t<decltype(storage)>>);
			}) || !succeed)
			{
				nat_Throw(InterpreterException, u8"无法创建声明的存储");
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
	InterpreterExprVisitor visitor{ m_Interpreter };
	nBool condition;
	if (!visitor.Evaluate(stmt->GetCond(), [&condition](nBool value)
	{
		condition = value;
	}, Expected<nBool>))
	{
		nat_Throw(InterpreterException, u8"条件表达式错误");
	}

	if (condition)
	{
		Visit(stmt->GetThen());
	}
	else
	{
		Visit(stmt->GetElse());
	}
}

void Interpreter::InterpreterStmtVisitor::VisitLabelStmt(natRefPointer<Statement::LabelStmt> const& stmt)
{
	Visit(stmt->GetSubStmt());
}

void Interpreter::InterpreterStmtVisitor::VisitNullStmt(natRefPointer<Statement::NullStmt> const& /*stmt*/)
{
}

void Interpreter::InterpreterStmtVisitor::VisitReturnStmt(natRefPointer<Statement::ReturnStmt> const& stmt)
{
	if (const auto retExpr = stmt->GetReturnExpr())
	{
		InterpreterExprVisitor visitor{ m_Interpreter };
		visitor.Visit(retExpr);
		m_ReturnedExpr = visitor.GetLastVisitedExpr();
	}

	m_Returned = true;
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
	const auto context = decl->GetContext();
	if (context)
	{
		context->RemoveDecl(decl);
	}

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
	m_Visitor->ResetReturnedExpr();
	m_DeclStorage.GarbageCollect();
}

natRefPointer<Semantic::Scope> Interpreter::GetScope() const noexcept
{
	return m_CurrentScope;
}
