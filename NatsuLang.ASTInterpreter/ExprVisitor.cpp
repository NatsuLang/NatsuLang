#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Detail;

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
	nat_Throw(InterpreterException, u8"此表达式无法被访问"_nv);
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
		m_Interpreter.m_Logger.LogMsg(u8"(表达式) {0}"_nv, expr->GetValue() ? u8"true"_nv : u8"false"_nv);
	}
}

void Interpreter::InterpreterExprVisitor::VisitCharacterLiteral(natRefPointer<Expression::CharacterLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg(u8"(表达式) '{0}'"_nv, U32StringView{ static_cast<U32StringView::CharType>(expr->GetCodePoint()) });
	}
}

void Interpreter::InterpreterExprVisitor::VisitDeclRefExpr(natRefPointer<Expression::DeclRefExpr> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		auto decl = expr->GetDecl();
		if (!m_Interpreter.m_DeclStorage.DoesDeclExist(decl))
		{
			nat_Throw(InterpreterException, u8"表达式引用了一个不存在的值定义"_nv);
		}

		const auto id = decl->GetIdentifierInfo();
		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(decl), [this, &id](auto value)
		{
			m_Interpreter.m_Logger.LogMsg(u8"(声明 : {0}) {1}", id ? id->GetName() : u8"(临时对象)", value);
		}, Excepted<InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))	// TODO: 应当允许这些访问器
		{
			nat_Throw(InterpreterException, u8"无法访问存储"_nv);
		}
	}
}

void Interpreter::InterpreterExprVisitor::VisitFloatingLiteral(natRefPointer<Expression::FloatingLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg(u8"(表达式) {0}"_nv, expr->GetValue());
	}
}

void Interpreter::InterpreterExprVisitor::VisitIntegerLiteral(natRefPointer<Expression::IntegerLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg(u8"(表达式) {0}"_nv, expr->GetValue());
	}
}

void Interpreter::InterpreterExprVisitor::VisitStringLiteral(natRefPointer<Expression::StringLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg(u8"(表达式) \"{0}\""_nv, expr->GetValue());
	}
}

void Interpreter::InterpreterExprVisitor::VisitNullPointerLiteral(natRefPointer<Expression::NullPointerLiteral> const& expr)
{
	VisitExpr(expr);
	if (m_ShouldPrint)
	{
		m_Interpreter.m_Logger.LogMsg(u8"(表达式) null"_nv);
	}
}

void Interpreter::InterpreterExprVisitor::VisitArraySubscriptExpr(natRefPointer<Expression::ArraySubscriptExpr> const& expr)
{
	Visit(expr->GetLeftOperand());
	const auto baseOperand = m_LastVisitedExpr.Cast<Expression::DeclRefExpr>();
	natRefPointer<Declaration::ValueDecl> baseDecl;
	natRefPointer<Type::ArrayType> baseType;
	if (baseOperand)
	{
		baseDecl = baseOperand->GetDecl();
		baseType = baseOperand->GetExprType();
	}

	if (!baseOperand || !baseDecl || !baseType)
	{
		nat_Throw(InterpreterException, u8"基础操作数无法被计算为有效的定义引用表达式"_nv);
	}

	Visit(expr->GetRightOperand());
	nuLong indexValue;
	if (!Evaluate(m_LastVisitedExpr, [&indexValue](auto value)
	{
		indexValue = value;
	}, Expected<nShort, nuShort, nInt, nuInt, nLong, nuLong>))
	{
		nat_Throw(InterpreterException, u8"下标操作数无法被计算为有效的整数值"_nv);
	}

	if (static_cast<std::size_t>(indexValue) >= baseType->GetSize())
	{
		nat_Throw(InterpreterException, u8"下标越界"_nv);
	}

	if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(baseDecl, [this, indexValue, &expr](InterpreterDeclStorage::ArrayElementAccessor const& accessor)
	{
		m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, accessor.GetElementDecl(indexValue), SourceLocation{}, expr->GetExprType());
	}, Expected<InterpreterDeclStorage::ArrayElementAccessor>))
	{
		nat_Throw(InterpreterException, u8"无法访问存储"_nv);
	}
}

void Interpreter::InterpreterExprVisitor::VisitConstructExpr(natRefPointer<Expression::ConstructExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitDeleteExpr(natRefPointer<Expression::DeleteExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitNewExpr(natRefPointer<Expression::NewExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitThisExpr(natRefPointer<Expression::ThisExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitThrowExpr(natRefPointer<Expression::ThrowExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitCallExpr(natRefPointer<Expression::CallExpr> const& expr)
{
	Visit(expr->GetCallee());
	const auto callee = m_LastVisitedExpr.Cast<Expression::DeclRefExpr>();
	if (!callee)
	{
		nat_Throw(InterpreterException, u8"被调用者错误"_nv);
	}

	if (const auto calleeDecl = callee->GetDecl().Cast<Declaration::FunctionDecl>())
	{
		if (!calleeDecl->GetBody())
		{
			nat_Throw(InterpreterException, u8"该函数无函数体，调用了声明为 extern 的函数？"_nv);
		}

		const auto args = expr->GetArgs();
		const auto params = calleeDecl->GetParams();

		m_Interpreter.m_DeclStorage.PushStorage(DeclStorageLevelFlag::AvailableForCreateStorage | DeclStorageLevelFlag::CreateStorageIfNotFound);

		const auto scope = make_scope([this]
		{
			m_Interpreter.m_DeclStorage.PopStorage();
		});

		// TODO: 允许默认参数
		assert(expr->GetArgCount() == calleeDecl->GetParamCount());

		for (auto&& param : params.zip(args))
		{
			if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(param.first, [this, &param](auto& storage)
			{
				m_Interpreter.m_DeclStorage.SetTopStorageFlag(DeclStorageLevelFlag::None);
				m_Interpreter.m_DeclStorage.PushStorage();
				const auto scope2 = make_scope([this]
				{
					m_Interpreter.m_DeclStorage.GarbageCollect();
					m_Interpreter.m_DeclStorage.MergeStorage();
					m_Interpreter.m_DeclStorage.SetTopStorageFlag(DeclStorageLevelFlag::AvailableForCreateStorage | DeclStorageLevelFlag::CreateStorageIfNotFound);
				});

				if (!Evaluate(param.second, [&storage](auto value)
				{
					storage = value;
				}, Expected<std::remove_reference_t<decltype(storage)>>))
				{
					nat_Throw(InterpreterException, u8"无法对操作数求值"_nv);
				}
			}, Excepted<InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))	// TODO: 应当允许这些访问器
			{
				nat_Throw(InterpreterException, u8"无法为参数的定义分配存储"_nv);
			}
		}

		m_Interpreter.m_DeclStorage.SetTopStorageFlag(DeclStorageLevelFlag::AvailableForCreateStorage | DeclStorageLevelFlag::AvailableForLookup);

		const auto iter = m_Interpreter.m_FunctionMap.find(calleeDecl);
		if (iter != m_Interpreter.m_FunctionMap.end())
		{
			auto decl = iter->second({ params.begin(), params.end() });
			auto declType = decl->GetValueType();
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(decl), SourceLocation{}, std::move(declType));
		}
		else
		{
			InterpreterStmtVisitor stmtVisitor{ m_Interpreter };
			stmtVisitor.Visit(calleeDecl->GetBody());
			m_LastVisitedExpr = stmtVisitor.GetReturnedExpr();
			if (!m_LastVisitedExpr)
			{
				const auto retType = static_cast<natRefPointer<Type::FunctionType>>(calleeDecl->GetValueType())->GetResultType().Cast<Type::BuiltinType>();
				if (!retType || retType->GetBuiltinClass() != Type::BuiltinType::Void)
				{
					nat_Throw(InterpreterException, u8"要求返回值的函数在控制流离开后未返回任何值"_nv);
				}
			}
		}

		return;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitMemberCallExpr(natRefPointer<Expression::MemberCallExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitCastExpr(natRefPointer<Expression::CastExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitAsTypeExpr(natRefPointer<Expression::AsTypeExpr> const& expr)
{
	Visit(expr->GetOperand());

	auto castToType = Type::Type::GetUnderlyingType(expr->GetExprType());
	auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(castToType);
	auto declRefExpr = make_ref<Expression::DeclRefExpr>(nullptr, tempObjDef, SourceLocation{}, std::move(castToType));

	if (Evaluate(m_LastVisitedExpr, [this, &tempObjDef](auto value)
	{
		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(tempObjDef), [value](auto& storage)
		{
			storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
		}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
		{
			nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
		}
	}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
	{
		m_LastVisitedExpr = std::move(declRefExpr);
		return;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitImplicitCastExpr(natRefPointer<Expression::ImplicitCastExpr> const& expr)
{
	Visit(expr->GetOperand());

	auto castToType = expr->GetExprType();
	auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(castToType);
	auto declRefExpr = make_ref<Expression::DeclRefExpr>(nullptr, tempObjDef, SourceLocation{}, castToType);

	if (Evaluate(m_LastVisitedExpr, [this, &tempObjDef](auto value)
	{
		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(tempObjDef), [value](auto& storage)
		{
			storage = static_cast<std::remove_reference_t<decltype(storage)>>(value);
		}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
		{
			nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
		}
	}, Expected<nBool, nShort, nuShort, nInt, nuInt, nLong, nuLong, nFloat, nDouble>))
	{
		m_LastVisitedExpr = std::move(declRefExpr);
		return;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitMemberExpr(natRefPointer<Expression::MemberExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitParenExpr(natRefPointer<Expression::ParenExpr> const& expr)
{
	Visit(expr->GetInnerExpr());
}

void Interpreter::InterpreterExprVisitor::VisitStmtExpr(natRefPointer<Expression::StmtExpr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitConditionalOperator(natRefPointer<Expression::ConditionalOperator> const& expr)
{
	Visit(expr->GetCondition());
	const auto cond = std::move(m_LastVisitedExpr);

	nBool condValue;
	if (Evaluate(cond, [&condValue](nBool value)
	{
		condValue = value;
	}, Expected<nBool>))
	{
		auto retDecl = InterpreterDeclStorage::CreateTemporaryObjectDecl(expr->GetExprType());
		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(retDecl, [this, condValue, &expr](auto&& value)
		{
			Visit(condValue ? expr->GetLeftOperand() : expr->GetRightOperand());
			if (!Evaluate(m_LastVisitedExpr, [&value](auto ret)
			{
				value = ret;
			}, Expected<std::remove_reference_t<decltype(value)>>))
			{
				nat_Throw(InterpreterException, u8"无法对表达式求值"_nv);
			}
		}, Excepted<InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
		{
			nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
		}

		m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(retDecl), SourceLocation{}, expr->GetExprType());
	}
}

void Interpreter::InterpreterExprVisitor::VisitBinaryOperator(natRefPointer<Expression::BinaryOperator> const& expr)
{
	const auto opCode = expr->GetOpcode();
	Visit(expr->GetLeftOperand());
	const auto leftOperand = std::move(m_LastVisitedExpr);
	Visit(expr->GetRightOperand());
	auto rightOperand = std::move(m_LastVisitedExpr);

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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"被0除"_nv);
				}

				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue / rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"被0除"_nv);
				}

				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [leftValue, rightValue](auto& storage)
				{
					storage = leftValue % rightValue;
				}, Expected<decltype(leftValue)>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nBool, nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}
			}, Expected<decltype(leftValue)>);
		}, Excepted<nByte, nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) && evalSucceed)
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

	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterExprVisitor::VisitCompoundAssignOperator(natRefPointer<Expression::CompoundAssignOperator> const& expr)
{
	const auto opCode = expr->GetOpcode();
	Visit(expr->GetLeftOperand());
	const auto leftOperand = std::move(m_LastVisitedExpr);
	const auto rightOperand = expr->GetRightOperand();

	auto leftDeclExpr = leftOperand.Cast<Expression::DeclRefExpr>();

	natRefPointer<Declaration::ValueDecl> decl;
	if (!leftDeclExpr || !((decl = leftDeclExpr->GetDecl())) || !decl->GetIdentifierInfo())
	{
		nat_Throw(InterpreterException, u8"左操作数必须是对非临时对象的定义的引用"_nv);
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
		}, Excepted<InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::MulAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage *= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::DivAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage /= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::RemAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage %= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::AddAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage += value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::SubAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage -= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::ShlAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage <<= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::ShrAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage >>= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::AndAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage &= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::XorAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage ^= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::OrAssign:
		visitSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &rightOperand, &evalSucceed](auto& storage)
		{
			evalSucceed = Evaluate(rightOperand, [&storage](auto value)
			{
				storage |= value;
			}, Expected<std::remove_reference_t<decltype(storage)>>);
		}, Excepted<nBool, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>);
		break;
	case Expression::BinaryOperationType::Invalid:
	default:
		nat_Throw(InterpreterException, u8"错误的操作"_nv);
	}

	if (!visitSucceed || !evalSucceed)
	{
		nat_Throw(InterpreterException, u8"操作失败"_nv);
	}

	m_LastVisitedExpr = std::move(leftDeclExpr);
}

// TODO: 在规范中定义对象被销毁的时机
void Interpreter::InterpreterExprVisitor::VisitUnaryOperator(natRefPointer<Expression::UnaryOperator> const& expr)
{
	const auto opCode = expr->GetOpcode();
	Visit(expr->GetOperand());
	const auto declExpr = m_LastVisitedExpr.Cast<Expression::DeclRefExpr>();

	switch (opCode)
	{
	case Expression::UnaryOperationType::PostInc:
		if (declExpr)
		{
			const auto decl = declExpr->GetDecl();
			if (!decl->GetIdentifierInfo())
			{
				nat_Throw(InterpreterException, u8"不允许修改临时对象"_nv);
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &decl](auto& value)
			{
				auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(decl->GetValueType());
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& tmpValue)
				{
					tmpValue = value;
				}, Expected<std::remove_reference_t<decltype(value)>>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}

				m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, decl->GetValueType());
				++value;
			}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
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
				nat_Throw(InterpreterException, u8"不允许修改临时对象"_nv);
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(decl, [this, &decl](auto& value)
			{
				auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(decl->GetValueType());
				if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [value](auto& tmpValue)
				{
					tmpValue = value;
				}, Expected<std::remove_reference_t<decltype(value)>>))
				{
					nat_Throw(InterpreterException, u8"无法创建临时对象的存储"_nv);
				}

				m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, decl->GetValueType());
				--value;
			}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
			{
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::PreInc:
		if (declExpr)
		{
			auto decl = declExpr->GetDecl();
			if (!decl->GetIdentifierInfo())
			{
				nat_Throw(InterpreterException, u8"不允许修改临时对象"_nv);
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(decl), [](auto& value)
			{
				++value;
			}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
			{
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::PreDec:
		if (declExpr)
		{
			auto decl = declExpr->GetDecl();
			if (!decl->GetIdentifierInfo())
			{
				nat_Throw(InterpreterException, u8"不允许修改临时对象"_nv);
			}

			if (m_Interpreter.m_DeclStorage.VisitDeclStorage(std::move(decl), [](auto& value)
			{
				--value;
			}, Excepted<nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
			{
				return;
			}
		}
		break;
	case Expression::UnaryOperationType::Plus:
	{
		auto type = m_LastVisitedExpr->GetExprType();
		auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(type);
		if (m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [this, expr = std::move(m_LastVisitedExpr)](auto& tmpValue)
		{
			if (!Evaluate(expr, [&tmpValue](auto value)
			{
				tmpValue = value;
			}, Expected<std::remove_reference_t<decltype(tmpValue)>>))
			{
				nat_Throw(InterpreterException, u8"无法对操作数求值"_nv);
			}
		}, Excepted<nStrView, nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, std::move(type));
			return;
		}

		break;
	}
	case Expression::UnaryOperationType::Minus:
	{
		auto type = m_LastVisitedExpr->GetExprType();
		auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(type);
		if (m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [this, expr = std::move(m_LastVisitedExpr)](auto& tmpValue)
		{
			if (!Evaluate(expr, [&tmpValue](auto value)
			{
				tmpValue = decltype(value){} -value;
			}, Expected<std::remove_reference_t<decltype(tmpValue)>>))
			{
				nat_Throw(InterpreterException, u8"无法对操作数求值"_nv);
			}
		}, Excepted<nStrView, nBool, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
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
				nat_Throw(InterpreterException, u8"无法对操作数求值"_nv);
			}
		}, Excepted<nStrView, nBool, nFloat, nDouble, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
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
			}, Excepted<nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
			{
				nat_Throw(InterpreterException, u8"无法对操作数求值"_nv);
			}
		}, Expected<nBool>))
		{
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, std::move(boolType));
			return;
		}

		break;
	}
	case Expression::UnaryOperationType::AddrOf:
	{
		auto pointerType = m_Interpreter.m_AstContext.GetPointerType(m_LastVisitedExpr->GetExprType());
		auto tempObjDef = InterpreterDeclStorage::CreateTemporaryObjectDecl(pointerType);
		if (!declExpr)
		{
			nat_Throw(InterpreterException, u8"该表达式未引用任何定义"_nv);
		}

		const auto decl = declExpr->GetDecl().Cast<Declaration::VarDecl>();
		if (!decl)
		{
			nat_Throw(InterpreterException, u8"该表达式引用的是临时对象的定义"_nv);
		}

		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDef, [decl](InterpreterDeclStorage::PointerAccessor& accessor)
		{
			accessor.SetReferencedDecl(decl);
		}, Expected<InterpreterDeclStorage::PointerAccessor>))
		{
			nat_Throw(InterpreterException, u8"无法对操作数求值"_nv);
		}

		m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(tempObjDef), SourceLocation{}, std::move(pointerType));
		return;
	}
	case Expression::UnaryOperationType::Deref:
	{
		if (!declExpr)
		{
			nat_Throw(InterpreterException, u8"该表达式未引用任何定义"_nv);
		}

		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(declExpr->GetDecl(), [this](InterpreterDeclStorage::PointerAccessor& accessor)
		{
			auto decl = accessor.GetReferencedDecl();
			auto declType = decl->GetValueType();
			m_LastVisitedExpr = make_ref<Expression::DeclRefExpr>(nullptr, std::move(decl), SourceLocation{}, std::move(declType));
		}, Expected<InterpreterDeclStorage::PointerAccessor>))
		{
			nat_Throw(InterpreterException, u8"无法对操作数求值"_nv);
		}

		return;
	}
	case Expression::UnaryOperationType::Invalid:
	default:
		break;
	}

	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}
