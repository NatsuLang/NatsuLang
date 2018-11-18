#include "AST/Expression.h"
#include "AST/Statement.h"
#include "AST/StmtVisitor.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

#undef max
#undef min

using namespace NatsuLib;
using namespace NatsuLang;
using namespace Statement;
using namespace Expression;

namespace
{
	nBool Evaluate(natRefPointer<Expr> const& expr, ASTContext& context, Expr::EvalResult& result);
	nBool EvaluateInteger(natRefPointer<Expr> const& expr, ASTContext& context, Expr::EvalResult& result);
	nBool EvaluateFloat(natRefPointer<Expr> const& expr, ASTContext& context, Expr::EvalResult& result);

	template <typename T>
	class ExprEvaluatorBase
		: public StmtVisitor<T, nBool>
	{
	public:
		ExprEvaluatorBase(ASTContext& context, Expr::EvalResult& result)
			: m_Context{ context }, m_Result{ result }
		{
		}

		nBool VisitStmt(natRefPointer<Stmt> const&)
		{
			return false;
		}

		nBool VisitParenExpr(natRefPointer<ParenExpr> const& expr)
		{
			return this->Visit(expr->GetInnerExpr());
		}

	protected:
		ASTContext& m_Context;
		Expr::EvalResult& m_Result;
	};

	// TODO: 未对不同位数的整数类型进行特别处理，可能在溢出后会得到意料不到的值
	class IntExprEvaluator
		: public ExprEvaluatorBase<IntExprEvaluator>
	{
	public:
		IntExprEvaluator(ASTContext& context, Expr::EvalResult& result)
			: ExprEvaluatorBase{ context, result }
		{
		}

		nBool VisitCharacterLiteral(natRefPointer<CharacterLiteral> const& expr)
		{
			m_Result.Result.emplace<0>(expr->GetCodePoint());
			return true;
		}

		nBool VisitIntegerLiteral(natRefPointer<IntegerLiteral> const& expr)
		{
			m_Result.Result.emplace<0>(expr->GetValue());
			return true;
		}

		nBool VisitBooleanLiteral(natRefPointer<BooleanLiteral> const& expr)
		{
			m_Result.Result.emplace<0>(static_cast<nuLong>(expr->GetValue()));
			return true;
		}

		nBool VisitCastExpr(natRefPointer<CastExpr> const& expr)
		{
			const auto operand = expr->GetOperand();
			/*auto destType = expr->GetType();
			auto srcType = operand->GetType();*/

			switch (expr->GetCastType())
			{
			case CastType::NoOp:
			case CastType::IntegralCast:
				return Visit(operand);
			case CastType::FloatingToIntegral:
			{
				Expr::EvalResult result;
				if (!EvaluateFloat(expr, m_Context, result) || result.Result.index() != 1)
				{
					return false;
				}

				m_Result.Result.emplace<0>(static_cast<nuLong>(std::get<1>(result.Result)));
				return true;
			}
			case CastType::IntegralToBoolean:
			case CastType::FloatingToBoolean:
			{
				Expr::EvalResult result;
				if (!Evaluate(expr, m_Context, result))
				{
					return false;
				}

				if (result.Result.index() == 0)
				{
					m_Result.Result.emplace<0>(static_cast<nBool>(std::get<0>(result.Result)));
				}
				else
				{
					m_Result.Result.emplace<0>(static_cast<nBool>(std::get<1>(result.Result)));
				}

				return true;
			}
			case CastType::IntegralToFloating:
			case CastType::FloatingCast:
			case CastType::Invalid:
			default:
				return false;
			}
		}

		// 当此操作数无法决定整个表达式的值的时候返回true
		static nBool VisitLogicalBinaryOperatorOperand(Expr::EvalResult& result, natRefPointer<BinaryOperator> const& expr)
		{
			const auto opcode = expr->GetOpcode();
			if (IsBinLogicalOp(opcode))
			{
				nBool value;
				if (result.GetResultAsBoolean(value))
				{
					if (value == (opcode == BinaryOperationType::LOr))
					{
						result.Result.emplace<0>(value);
						return false;
					}
				}
			}

			return true;
		}

		// TODO: 处理浮点类型的比较操作
		nBool VisitBinaryOperator(natRefPointer<BinaryOperator> const& expr)
		{
			const auto opcode = expr->GetOpcode();
			const auto leftOperand = expr->GetLeftOperand();
			const auto rightOperand = expr->GetRightOperand();

			Expr::EvalResult leftResult, rightResult;

			if (!Evaluate(leftOperand, m_Context, leftResult))
			{
				return false;
			}

			// 如果是逻辑操作符，判断是否可以短路求值
			if (IsBinLogicalOp(opcode))
			{
				nBool value;
				if (VisitLogicalBinaryOperatorOperand(leftResult, expr) && Evaluate(rightOperand, m_Context, rightResult) && rightResult.GetResultAsBoolean(value))
				{
					m_Result.Result.emplace<0>(value);
					return true;
				}

				return false;
			}

			// 右操作数需要求值，因为如果已经进行了求值操作则不会到达此处
			if (!Evaluate(rightOperand, m_Context, rightResult))
			{
				return false;
			}

			if (leftResult.Result.index() != 0 || rightResult.Result.index() != 0)
			{
				return false;
			}

			auto leftValue = std::get<0>(leftResult.Result), rightValue = std::get<0>(rightResult.Result);

			// 无需判断逻辑操作符的情况
			switch (opcode)
			{
			case BinaryOperationType::Mul:
				m_Result.Result.emplace<0>(leftValue * rightValue);
				return true;
			case BinaryOperationType::Add:
				m_Result.Result.emplace<0>(leftValue + rightValue);
				return true;
			case BinaryOperationType::Sub:
				m_Result.Result.emplace<0>(leftValue - rightValue);
				return true;
			case BinaryOperationType::Div:
				if (rightValue == 0)
				{
					return false;
				}
				m_Result.Result.emplace<0>(leftValue / rightValue);
				return true;
			case BinaryOperationType::Rem:
				if (rightValue == 0)
				{
					return false;
				}
				m_Result.Result.emplace<0>(leftValue % rightValue);
				return true;
			case BinaryOperationType::Shl:
				// 溢出
				if (rightValue >= sizeof(nuLong) * 8)
				{
					return false;
				}
				m_Result.Result.emplace<0>(leftValue << rightValue);
				return true;
			case BinaryOperationType::Shr:
				m_Result.Result.emplace<0>(leftValue >> rightValue);
				return true;
			// TODO: 对于比较操作符，若其一操作数曾经溢出，则结果可能出现异常
			case BinaryOperationType::LT:
				m_Result.Result.emplace<0>(leftValue < rightValue);
				return true;
			case BinaryOperationType::GT:
				m_Result.Result.emplace<0>(leftValue > rightValue);
				return true;
			case BinaryOperationType::LE:
				m_Result.Result.emplace<0>(leftValue <= rightValue);
				return true;
			case BinaryOperationType::GE:
				m_Result.Result.emplace<0>(leftValue >= rightValue);
				return true;
			case BinaryOperationType::EQ:
				m_Result.Result.emplace<0>(leftValue == rightValue);
				return true;
			case BinaryOperationType::NE:
				m_Result.Result.emplace<0>(leftValue != rightValue);
				return true;
			case BinaryOperationType::And:
				m_Result.Result.emplace<0>(leftValue & rightValue);
				return true;
			case BinaryOperationType::Xor:
				m_Result.Result.emplace<0>(leftValue ^ rightValue);
				return true;
			case BinaryOperationType::Or:
				m_Result.Result.emplace<0>(leftValue | rightValue);
				return true;
			case BinaryOperationType::Assign:
			case BinaryOperationType::MulAssign:
			case BinaryOperationType::DivAssign:
			case BinaryOperationType::RemAssign:
			case BinaryOperationType::AddAssign:
			case BinaryOperationType::SubAssign:
			case BinaryOperationType::ShlAssign:
			case BinaryOperationType::ShrAssign:
			case BinaryOperationType::AndAssign:
			case BinaryOperationType::XorAssign:
			case BinaryOperationType::OrAssign:
				// 不能常量折叠
			case BinaryOperationType::Invalid:
			default:
				return false;
			}
		}

		nBool VisitUnaryOperator(natRefPointer<UnaryOperator> const& expr)
		{
			switch (expr->GetOpcode())
			{
			case UnaryOperationType::Plus:
				return Visit(expr->GetOperand());
			case UnaryOperationType::Minus:
				if (!Visit(expr->GetOperand()))
				{
					return false;
				}

				if (m_Result.Result.index() != 0)
				{
					return false;
				}

				m_Result.Result.emplace<0>(std::numeric_limits<nuLong>::max() - std::get<0>(m_Result.Result));
				return true;
			case UnaryOperationType::Not:
				if (!Visit(expr->GetOperand()))
				{
					return false;
				}

				if (m_Result.Result.index() != 0)
				{
					return false;
				}

				m_Result.Result.emplace<0>(~std::get<0>(m_Result.Result));
				return true;
			case UnaryOperationType::LNot:
				if (!Visit(expr->GetOperand()))
				{
					return false;
				}

				if (m_Result.Result.index() != 0)
				{
					return false;
				}

				m_Result.Result.emplace<0>(!std::get<0>(m_Result.Result));
				return true;
			case UnaryOperationType::Invalid:
			case UnaryOperationType::PostInc:
			case UnaryOperationType::PostDec:
			case UnaryOperationType::PreInc:
			case UnaryOperationType::PreDec:
			default:
				return false;
			}
		}

		nBool VisitConditionalOperator(natRefPointer<ConditionalOperator> const& expr)
		{
			if (!Visit(expr->GetCondition()))
			{
				return false;
			}

			if (m_Result.Result.index() != 0)
			{
				return false;
			}

			return Visit(std::get<0>(m_Result.Result) ? expr->GetLeftOperand() : expr->GetRightOperand());
		}

		nBool VisitDeclRefExpr(natRefPointer<DeclRefExpr> const& expr)
		{
			const auto decl = expr->GetDecl();

			if (const auto enumeratorDecl = decl.Cast<Declaration::EnumConstantDecl>())
			{
				m_Result.Result.emplace<0>(enumeratorDecl->GetValue());
				return true;
			}

			if (const auto varDecl = decl.Cast<Declaration::VarDecl>(); varDecl && varDecl->GetStorageClass() == Specifier::StorageClass::Const)
			{
				return Visit(varDecl->GetInitializer());
			}

			return false;
		}
	};

	class FloatExprEvaluator
		: public ExprEvaluatorBase<FloatExprEvaluator>
	{
	public:
		FloatExprEvaluator(ASTContext& context, Expr::EvalResult& result)
			: ExprEvaluatorBase{ context, result }
		{
		}

		nBool VisitFloatingLiteral(natRefPointer<FloatingLiteral> const& expr)
		{
			m_Result.Result.emplace<1>(expr->GetValue());
			return true;
		}

		nBool VisitCastExpr(natRefPointer<CastExpr> const& expr)
		{
			const auto operand = expr->GetOperand();

			switch (expr->GetCastType())
			{
			case CastType::NoOp:
			case CastType::FloatingCast:
				return Visit(operand);
			case CastType::IntegralToFloating:
			{
				Expr::EvalResult result;
				if (!EvaluateInteger(operand, m_Context, result) || result.Result.index() != 0)
				{
					return false;
				}

				m_Result.Result.emplace<1>(static_cast<nDouble>(std::get<0>(result.Result)));
				return true;
			}
			case CastType::Invalid:
			case CastType::IntegralCast:
			case CastType::IntegralToBoolean:
			case CastType::FloatingToIntegral:
			case CastType::FloatingToBoolean:
			default:
				return false;
			}
		}

		nBool VisitBinaryOperator(natRefPointer<BinaryOperator> const& expr)
		{
			const auto opcode = expr->GetOpcode();
			const auto leftOperand = expr->GetLeftOperand();
			const auto rightOperand = expr->GetRightOperand();

			Expr::EvalResult leftResult, rightResult;

			if (!Evaluate(leftOperand, m_Context, leftResult) || leftResult.Result.index() != 1 ||
				!Evaluate(rightOperand, m_Context, rightResult) || rightResult.Result.index() != 1)
			{
				return false;
			}

			const auto leftValue = std::get<1>(leftResult.Result), rightValue = std::get<1>(rightResult.Result);
			switch (opcode)
			{
			case BinaryOperationType::Mul:
				m_Result.Result.emplace<1>(leftValue * rightValue);
				return true;
			case BinaryOperationType::Div:
				m_Result.Result.emplace<1>(leftValue / rightValue);
				return true;
			case BinaryOperationType::Add:
				m_Result.Result.emplace<1>(leftValue + rightValue);
				return true;
			case BinaryOperationType::Sub:
				m_Result.Result.emplace<1>(leftValue - rightValue);
				return true;
			case BinaryOperationType::Invalid:
			case BinaryOperationType::Rem:
			case BinaryOperationType::Shl:
			case BinaryOperationType::Shr:
			case BinaryOperationType::LT:
			case BinaryOperationType::GT:
			case BinaryOperationType::LE:
			case BinaryOperationType::GE:
			case BinaryOperationType::EQ:
			case BinaryOperationType::NE:
			case BinaryOperationType::And:
			case BinaryOperationType::Xor:
			case BinaryOperationType::Or:
			case BinaryOperationType::LAnd:
			case BinaryOperationType::LOr:
			case BinaryOperationType::Assign:
			case BinaryOperationType::MulAssign:
			case BinaryOperationType::DivAssign:
			case BinaryOperationType::RemAssign:
			case BinaryOperationType::AddAssign:
			case BinaryOperationType::SubAssign:
			case BinaryOperationType::ShlAssign:
			case BinaryOperationType::ShrAssign:
			case BinaryOperationType::AndAssign:
			case BinaryOperationType::XorAssign:
			case BinaryOperationType::OrAssign:
			default:
				return false;
			}
		}

		nBool VisitUnaryOperator(natRefPointer<UnaryOperator> const& expr)
		{
			switch (expr->GetOpcode())
			{
			case UnaryOperationType::Plus:
				return EvaluateFloat(expr->GetOperand(), m_Context, m_Result);
			case UnaryOperationType::Minus:
				if (!EvaluateFloat(expr->GetOperand(), m_Context, m_Result) || m_Result.Result.index() != 1)
				{
					return false;
				}

				m_Result.Result.emplace<1>(-std::get<1>(m_Result.Result));
				return true;
			default:
				return false;
			}
		}

		nBool VisitConditionalOperator(natRefPointer<ConditionalOperator> const& expr)
		{
			if (!Visit(expr->GetCondition()))
			{
				return false;
			}

			if (m_Result.Result.index() != 0)
			{
				return false;
			}

			return Visit(std::get<0>(m_Result.Result) ? expr->GetLeftOperand() : expr->GetRightOperand());
		}

		nBool VisitDeclRefExpr(natRefPointer<DeclRefExpr> const& expr)
		{
			const auto decl = expr->GetDecl();

			if (const auto enumeratorDecl = decl.Cast<Declaration::EnumConstantDecl>())
			{
				m_Result.Result.emplace<1>(static_cast<nDouble>(enumeratorDecl->GetValue()));
				return true;
			}

			if (const auto varDecl = decl.Cast<Declaration::VarDecl>(); varDecl && varDecl->GetStorageClass() == Specifier::StorageClass::Const)
			{
				return Visit(varDecl->GetInitializer());
			}

			return false;
		}
	};

	nBool Evaluate(natRefPointer<Expr> const& expr, ASTContext& context, Expr::EvalResult& result)
	{
		const auto type = expr->GetExprType();

		if (const auto builtinType = type.Cast<Type::BuiltinType>())
		{
			if (builtinType->IsIntegerType())
			{
				return EvaluateInteger(expr, context, result);
			}

			if (builtinType->IsFloatingType())
			{
				return EvaluateFloat(expr, context, result);
			}

			return false;
		}

		if (type->GetType() == Type::Type::Enum)
		{
			return EvaluateInteger(expr, context, result);
		}

		return false;
	}

	nBool EvaluateInteger(natRefPointer<Expr> const& expr, ASTContext& context, Expr::EvalResult& result)
	{
		IntExprEvaluator evaluator{ context, result };
		return evaluator.Visit(expr);
	}

	nBool EvaluateFloat(natRefPointer<Expr> const& expr, ASTContext& context, Expr::EvalResult& result)
	{
		FloatExprEvaluator evaluator{ context, result };
		return evaluator.Visit(expr);
	}
}

Expr::Expr(StmtType stmtType, Type::TypePtr exprType, ValueCategory valueCategory, SourceLocation start, SourceLocation end)
	: Stmt{ stmtType, start, end }, m_ExprType{ std::move(exprType) }, m_ValueCategory{ valueCategory }
{
}

Expr::~Expr()
{
}

ExprPtr Expr::IgnoreParens() noexcept
{
	auto ret = ForkRef<Expr>();
	// 可能的死循环
	while (true)
	{
		if (const auto parenExpr = ret.Cast<ParenExpr>())
		{
			ret = parenExpr->GetInnerExpr();
		}
		else
		{
			return ret;
		}
	}
}

nBool Expr::EvalResult::GetResultAsSignedInteger(nLong& result) const noexcept
{
	if (Result.index() != 0)
	{
		return false;
	}

	constexpr auto longMax = static_cast<nuLong>(std::numeric_limits<nLong>::max());
	const auto storedValue = std::get<0>(Result);
	result = storedValue > longMax ?
		-static_cast<nLong>(storedValue - longMax) :
		static_cast<nLong>(storedValue);

	return true;
}

nBool Expr::EvalResult::GetResultAsBoolean(nBool& result) const noexcept
{
	if (Result.index() == 0)
	{
		result = !!std::get<0>(Result);
	}
	else
	{
		result = !!std::get<1>(Result);
	}

	return true;
}

nBool Expr::Evaluate(EvalResult& result, ASTContext& context)
{
	return ::Evaluate(ForkRef<Expr>(), context, result);
}

nBool Expr::EvaluateAsInt(nuLong& result, ASTContext& context)
{
	EvalResult evalResult;
	if (!EvaluateInteger(ForkRef<Expr>(), context, evalResult) || evalResult.Result.index() != 0)
	{
		return false;
	}

	result = std::get<0>(evalResult.Result);
	return true;
}

nBool Expr::EvaluateAsFloat(nDouble& result, ASTContext& context)
{
	EvalResult evalResult;
	if (!EvaluateFloat(ForkRef<Expr>(), context, evalResult) || evalResult.Result.index() != 1)
	{
		return false;
	}

	result = std::get<1>(evalResult.Result);
	return true;
}

DeclRefExpr::~DeclRefExpr()
{
}

IntegerLiteral::~IntegerLiteral()
{
}

CharacterLiteral::~CharacterLiteral()
{
}

FloatingLiteral::~FloatingLiteral()
{
}

StringLiteral::~StringLiteral()
{
}

NullPointerLiteral::~NullPointerLiteral()
{
}

BooleanLiteral::~BooleanLiteral()
{
}

ParenExpr::~ParenExpr()
{
}

StmtEnumerable ParenExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_InnerExpr) });
}

UnaryOperator::~UnaryOperator()
{
}

StmtEnumerable UnaryOperator::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Operand) });
}

ArraySubscriptExpr::~ArraySubscriptExpr()
{
}

StmtEnumerable ArraySubscriptExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_LeftOperand), static_cast<StmtPtr>(m_RightOperand) });
}

CallExpr::~CallExpr()
{
}

Linq<Valued<ExprPtr>> CallExpr::GetArgs() const noexcept
{
	return from(m_Args);
}

void CallExpr::SetArgs(Linq<Valued<ExprPtr>> const& value)
{
	m_Args.assign(value.begin(), value.end());
}

StmtEnumerable CallExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Function) }).concat(from(m_Args).select([](ExprPtr const& expr) { return static_cast<StmtPtr>(expr); }));
}

MemberExpr::~MemberExpr()
{
}

StmtEnumerable MemberExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Base) });
}

MemberCallExpr::~MemberCallExpr()
{
}

ExprPtr MemberCallExpr::GetImplicitObjectArgument() const noexcept
{
	const auto callee = GetCallee().Cast<MemberExpr>();
	if (callee)
	{
		return callee->GetBase();
	}

	return nullptr;
}

CastExpr::~CastExpr()
{
}

StmtEnumerable CastExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Operand) });
}

ImplicitCastExpr::~ImplicitCastExpr()
{
}

AsTypeExpr::~AsTypeExpr()
{
}

InitListExpr::~InitListExpr()
{
}

Linq<Valued<ExprPtr>> InitListExpr::GetInitExprs() const noexcept
{
	return from(m_InitExprs);
}

BinaryOperator::~BinaryOperator()
{
}

StmtEnumerable BinaryOperator::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_LeftOperand), static_cast<StmtPtr>(m_RightOperand) });
}

CompoundAssignOperator::~CompoundAssignOperator()
{
}

ConditionalOperator::~ConditionalOperator()
{
}

StmtEnumerable ConditionalOperator::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Condition), static_cast<StmtPtr>(m_LeftOperand), static_cast<StmtPtr>(m_RightOperand) });
}

StmtExpr::~StmtExpr()
{
}

StmtEnumerable StmtExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_SubStmt) });
}

ThisExpr::~ThisExpr()
{
}

ThrowExpr::~ThrowExpr()
{
}

StmtEnumerable ThrowExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Operand) });
}

ConstructExpr::~ConstructExpr()
{
}

Linq<Valued<ExprPtr>> ConstructExpr::GetArgs() const noexcept
{
	return from(m_Args);
}

std::size_t ConstructExpr::GetArgCount() const noexcept
{
	return m_Args.size();
}

void ConstructExpr::SetArgs(Linq<Valued<ExprPtr>> const& value)
{
	m_Args.assign(value.begin(), value.end());
}

void ConstructExpr::SetArgs(std::vector<ExprPtr> value)
{
	m_Args = std::move(value);
}

StmtEnumerable ConstructExpr::GetChildrenStmt()
{
	return from(m_Args).select([](ExprPtr const& expr) { return static_cast<StmtPtr>(expr); });
}

NewExpr::~NewExpr()
{
}

Linq<Valued<ExprPtr>> NewExpr::GetArgs() const noexcept
{
	return from(m_Args);
}

void NewExpr::SetArgs(Linq<Valued<ExprPtr>> const& value)
{
	m_Args.assign(value.begin(), value.end());
}

StmtEnumerable NewExpr::GetChildrenStmt()
{
	return from(m_Args).select([](ExprPtr const& expr) { return static_cast<StmtPtr>(expr); });
}

DeleteExpr::~DeleteExpr()
{
}

StmtEnumerable DeleteExpr::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Operand) });
}
