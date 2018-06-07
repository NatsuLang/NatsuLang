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

	class ExprEvaluatorBase
		: public natRefObjImpl<ExprEvaluatorBase, StmtVisitor>
	{
	public:
		ExprEvaluatorBase(ASTContext& context, Expr::EvalResult& result)
			: m_Context{ context }, m_Result{ result }, m_LastVisitSucceed{ false }
		{
		}

		void VisitStmt(natRefPointer<Stmt> const&) override
		{
			m_LastVisitSucceed = false;
		}

		void VisitParenExpr(natRefPointer<ParenExpr> const& expr) override
		{
			Visit(expr->GetInnerExpr());
		}

		nBool IsLastVisitSucceed() const noexcept
		{
			return m_LastVisitSucceed;
		}

	protected:
		ASTContext& m_Context;
		Expr::EvalResult& m_Result;
		nBool m_LastVisitSucceed;
	};

	// TODO: 未对不同位数的整数类型进行特别处理，可能在溢出后会得到意料不到的值
	class IntExprEvaluator
		: public ExprEvaluatorBase
	{
	public:
		IntExprEvaluator(ASTContext& context, Expr::EvalResult& result)
			: ExprEvaluatorBase{ context, result }
		{
		}

		void VisitCharacterLiteral(natRefPointer<CharacterLiteral> const& expr) override
		{
			m_Result.Result.emplace<0>(expr->GetCodePoint());
			m_LastVisitSucceed = true;
		}

		void VisitIntegerLiteral(natRefPointer<IntegerLiteral> const& expr) override
		{
			m_Result.Result.emplace<0>(expr->GetValue());
			m_LastVisitSucceed = true;
		}

		void VisitBooleanLiteral(natRefPointer<BooleanLiteral> const& expr) override
		{
			m_Result.Result.emplace<0>(static_cast<nuLong>(expr->GetValue()));
			m_LastVisitSucceed = true;
		}

		void VisitCastExpr(natRefPointer<CastExpr> const& expr) override
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
					m_LastVisitSucceed = false;
					return;
				}

				m_Result.Result.emplace<0>(static_cast<nuLong>(std::get<1>(result.Result)));
				m_LastVisitSucceed = true;
				return;
			}
			case CastType::IntegralToBoolean:
			case CastType::FloatingToBoolean:
			{
				Expr::EvalResult result;
				if (!Evaluate(expr, m_Context, result))
				{
					m_LastVisitSucceed = false;
					return;
				}

				if (result.Result.index() == 0)
				{
					m_Result.Result.emplace<0>(static_cast<nBool>(std::get<0>(result.Result)));
				}
				else
				{
					m_Result.Result.emplace<0>(static_cast<nBool>(std::get<1>(result.Result)));
				}

				m_LastVisitSucceed = true;
				return;
			}
			case CastType::IntegralToFloating:
			case CastType::FloatingCast:
			case CastType::Invalid:
			default:
				m_LastVisitSucceed = false;
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
		void VisitBinaryOperator(natRefPointer<BinaryOperator> const& expr) override
		{
			const auto opcode = expr->GetOpcode();
			const auto leftOperand = expr->GetLeftOperand();
			const auto rightOperand = expr->GetRightOperand();

			Expr::EvalResult leftResult, rightResult;

			if (!Evaluate(leftOperand, m_Context, leftResult))
			{
				m_LastVisitSucceed = false;
				return;
			}

			// 如果是逻辑操作符，判断是否可以短路求值
			if (IsBinLogicalOp(opcode))
			{
				nBool value;
				if (VisitLogicalBinaryOperatorOperand(leftResult, expr) && Evaluate(rightOperand, m_Context, rightResult) && rightResult.GetResultAsBoolean(value))
				{
					m_Result.Result.emplace<0>(value);
					m_LastVisitSucceed = true;
					return;
				}

				m_LastVisitSucceed = false;
				return;
			}

			// 右操作数需要求值，因为如果已经进行了求值操作则不会到达此处
			if (!Evaluate(rightOperand, m_Context, rightResult))
			{
				m_LastVisitSucceed = false;
				return;
			}

			if (leftResult.Result.index() != 0 || rightResult.Result.index() != 0)
			{
				m_LastVisitSucceed = false;
				return;
			}

			auto leftValue = std::get<0>(leftResult.Result), rightValue = std::get<0>(rightResult.Result);

			// 无需判断逻辑操作符的情况
			switch (opcode)
			{
			case BinaryOperationType::Mul:
				m_Result.Result.emplace<0>(leftValue * rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Add:
				m_Result.Result.emplace<0>(leftValue + rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Sub:
				m_Result.Result.emplace<0>(leftValue - rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Div:
				if (rightValue == 0)
				{
					m_LastVisitSucceed = false;
					return;
				}
				m_Result.Result.emplace<0>(leftValue / rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Rem:
				if (rightValue == 0)
				{
					m_LastVisitSucceed = false;
					return;
				}
				m_Result.Result.emplace<0>(leftValue % rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Shl:
				// 溢出
				if (rightValue >= sizeof(nuLong) * 8)
				{
					m_LastVisitSucceed = false;
					return;
				}
				m_Result.Result.emplace<0>(leftValue << rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Shr:
				m_Result.Result.emplace<0>(leftValue >> rightValue);
				m_LastVisitSucceed = true;
				return;
			// TODO: 对于比较操作符，若其一操作数曾经溢出，则结果可能出现异常
			case BinaryOperationType::LT:
				m_Result.Result.emplace<0>(leftValue < rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::GT:
				m_Result.Result.emplace<0>(leftValue > rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::LE:
				m_Result.Result.emplace<0>(leftValue <= rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::GE:
				m_Result.Result.emplace<0>(leftValue >= rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::EQ:
				m_Result.Result.emplace<0>(leftValue == rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::NE:
				m_Result.Result.emplace<0>(leftValue != rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::And:
				m_Result.Result.emplace<0>(leftValue & rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Xor:
				m_Result.Result.emplace<0>(leftValue ^ rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Or:
				m_Result.Result.emplace<0>(leftValue | rightValue);
				m_LastVisitSucceed = true;
				return;
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
				m_LastVisitSucceed = false;
			}
		}

		void VisitUnaryOperator(natRefPointer<UnaryOperator> const& expr) override
		{
			switch (expr->GetOpcode())
			{
			case UnaryOperationType::Plus:
				Visit(expr->GetOperand());
				return;
			case UnaryOperationType::Minus:
				Visit(expr->GetOperand());
				if (!m_LastVisitSucceed)
				{
					return;
				}

				if (m_Result.Result.index() != 0)
				{
					m_LastVisitSucceed = false;
					return;
				}

				m_Result.Result.emplace<0>(std::numeric_limits<nuLong>::max() - std::get<0>(m_Result.Result));
				m_LastVisitSucceed = true;
				return;
			case UnaryOperationType::Not:
				Visit(expr->GetOperand());
				if (!m_LastVisitSucceed)
				{
					return;
				}

				if (m_Result.Result.index() != 0)
				{
					m_LastVisitSucceed = false;
					return;
				}

				m_Result.Result.emplace<0>(~std::get<0>(m_Result.Result));
				m_LastVisitSucceed = true;
				return;
			case UnaryOperationType::LNot:
				Visit(expr->GetOperand());
				if (!m_LastVisitSucceed)
				{
					return;
				}

				if (m_Result.Result.index() != 0)
				{
					m_LastVisitSucceed = false;
					return;
				}

				m_Result.Result.emplace<0>(!std::get<0>(m_Result.Result));
				m_LastVisitSucceed = true;
				return;
			case UnaryOperationType::Invalid:
			case UnaryOperationType::PostInc:
			case UnaryOperationType::PostDec:
			case UnaryOperationType::PreInc:
			case UnaryOperationType::PreDec:
			default:
				m_LastVisitSucceed = false;
			}
		}

		void VisitConditionalOperator(natRefPointer<ConditionalOperator> const& expr) override
		{
			Visit(expr->GetCondition());
			if (!m_LastVisitSucceed)
			{
				return;
			}

			if (m_Result.Result.index() != 0)
			{
				m_LastVisitSucceed = false;
				return;
			}

			Visit(std::get<0>(m_Result.Result) ? expr->GetLeftOperand() : expr->GetRightOperand());
		}

		void VisitDeclRefExpr(natRefPointer<DeclRefExpr> const& expr) override
		{
			const auto decl = expr->GetDecl();

			if (const auto enumeratorDecl = decl.Cast<Declaration::EnumConstantDecl>())
			{
				m_Result.Result.emplace<0>(enumeratorDecl->GetValue());
				m_LastVisitSucceed = true;
			}

			// TODO: 支持常量成员
			m_LastVisitSucceed = false;
		}
	};

	class FloatExprEvaluator
		: public ExprEvaluatorBase
	{
	public:
		FloatExprEvaluator(ASTContext& context, Expr::EvalResult& result)
			: ExprEvaluatorBase{ context, result }
		{
		}

		void VisitFloatingLiteral(natRefPointer<FloatingLiteral> const& expr) override
		{
			m_Result.Result.emplace<1>(expr->GetValue());
			m_LastVisitSucceed = true;
		}

		void VisitCastExpr(natRefPointer<CastExpr> const& expr) override
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
					m_LastVisitSucceed = false;
					return;
				}

				m_Result.Result.emplace<1>(static_cast<nDouble>(std::get<0>(result.Result)));
				m_LastVisitSucceed = true;
				return;
			}
			case CastType::Invalid:
			case CastType::IntegralCast:
			case CastType::IntegralToBoolean:
			case CastType::FloatingToIntegral:
			case CastType::FloatingToBoolean:
			default:
				m_LastVisitSucceed = false;
			}
		}

		void VisitBinaryOperator(natRefPointer<BinaryOperator> const& expr) override
		{
			const auto opcode = expr->GetOpcode();
			const auto leftOperand = expr->GetLeftOperand();
			const auto rightOperand = expr->GetRightOperand();

			Expr::EvalResult leftResult, rightResult;

			if (!Evaluate(leftOperand, m_Context, leftResult) || leftResult.Result.index() != 1 ||
				!Evaluate(rightOperand, m_Context, rightResult) || rightResult.Result.index() != 1)
			{
				m_LastVisitSucceed = false;
				return;
			}

			const auto leftValue = std::get<1>(leftResult.Result), rightValue = std::get<1>(rightResult.Result);
			switch (opcode)
			{
			case BinaryOperationType::Mul:
				m_Result.Result.emplace<1>(leftValue * rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Div:
				m_Result.Result.emplace<1>(leftValue / rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Add:
				m_Result.Result.emplace<1>(leftValue + rightValue);
				m_LastVisitSucceed = true;
				return;
			case BinaryOperationType::Sub:
				m_Result.Result.emplace<1>(leftValue - rightValue);
				m_LastVisitSucceed = true;
				return;
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
				m_LastVisitSucceed = false;
			}
		}

		void VisitUnaryOperator(natRefPointer<UnaryOperator> const& expr) override
		{
			switch (expr->GetOpcode())
			{
			case UnaryOperationType::Plus:
				m_LastVisitSucceed = EvaluateFloat(expr->GetOperand(), m_Context, m_Result);
				return;
			case UnaryOperationType::Minus:
				if (!EvaluateFloat(expr->GetOperand(), m_Context, m_Result) || m_Result.Result.index() != 1)
				{
					m_LastVisitSucceed = false;
					return;
				}

				m_Result.Result.emplace<1>(-std::get<1>(m_Result.Result));
				m_LastVisitSucceed = true;
				return;
			default:
				m_LastVisitSucceed = false;
			}
		}

		void VisitConditionalOperator(natRefPointer<ConditionalOperator> const& expr) override
		{
			Visit(expr->GetCondition());
			if (!m_LastVisitSucceed)
			{
				return;
			}

			if (m_Result.Result.index() != 0)
			{
				m_LastVisitSucceed = false;
				return;
			}

			Visit(std::get<0>(m_Result.Result) ? expr->GetLeftOperand() : expr->GetRightOperand());
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
		evaluator.Visit(expr);
		return evaluator.IsLastVisitSucceed();
	}

	nBool EvaluateFloat(natRefPointer<Expr> const& expr, ASTContext& context, Expr::EvalResult& result)
	{
		FloatExprEvaluator evaluator{ context, result };
		evaluator.Visit(expr);
		return evaluator.IsLastVisitSucceed();
	}
}

Expr::Expr(StmtType stmtType, Type::TypePtr exprType, SourceLocation start, SourceLocation end)
	: Stmt{ stmtType, start, end }, m_ExprType{ std::move(exprType) }
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

#define DEFAULT_ACCEPT_DEF(Type) void Type::Accept(NatsuLib::natRefPointer<StmtVisitor> const& visitor) { visitor->Visit##Type(ForkRef<Type>()); }
#define STMT(StmtType, Base)
#define EXPR(ExprType, Base) DEFAULT_ACCEPT_DEF(ExprType)
#include "Basic/StmtDef.h"
