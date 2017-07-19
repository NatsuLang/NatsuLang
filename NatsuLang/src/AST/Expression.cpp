#include "AST/Expression.h"

using namespace NatsuLib;
using namespace NatsuLang::Statement;
using namespace NatsuLang::Expression;

Expr::Expr(StmtType stmtType, Type::TypePtr exprType, SourceLocation start, SourceLocation end)
	: Stmt{ stmtType, start, end }, m_ExprType{ std::move(exprType) }
{
}

Expr::~Expr()
{
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

BooleanLiteral::~BooleanLiteral()
{
}

ParenExpr::~ParenExpr()
{
}

StmtEnumerable ParenExpr::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_InnerExpr));
}

UnaryOperator::~UnaryOperator()
{
}

StmtEnumerable UnaryOperator::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_Operand));
}

UnaryExprOrTypeTraitExpr::~UnaryExprOrTypeTraitExpr()
{
}

StmtEnumerable UnaryExprOrTypeTraitExpr::GetChildrens()
{
	if (m_Operand.index() == 0)
	{
		return Expr::GetChildrens();
	}

	return from_values(static_cast<StmtPtr>(std::get<1>(m_Operand)));
}

ArraySubscriptExpr::~ArraySubscriptExpr()
{
}

StmtEnumerable ArraySubscriptExpr::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_LeftOperand), static_cast<StmtPtr>(m_RightOperand));
}

CallExpr::~CallExpr()
{
}

Linq<NatsuLang::Expression::ExprPtr> CallExpr::GetArgs() const noexcept
{
	return from(m_Args);
}

void CallExpr::SetArgs(Linq<ExprPtr> const& value)
{
	m_Args.assign(value.begin(), value.end());
}

StmtEnumerable CallExpr::GetChildrens()
{
	return from_values(m_Function).concat(from(m_Args)).select([](ExprPtr const& expr) { return static_cast<StmtPtr>(expr); });
}

MemberExpr::~MemberExpr()
{
}

StmtEnumerable MemberExpr::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_Base));
}

MemberCallExpr::~MemberCallExpr()
{
}

NatsuLang::Expression::ExprPtr MemberCallExpr::GetImplicitObjectArgument() const noexcept
{
	const auto callee = static_cast<natRefPointer<MemberExpr>>(GetCallee());
	if (callee)
	{
		return callee->GetBase();
	}

	return nullptr;
}

CastExpr::~CastExpr()
{
}

StmtEnumerable CastExpr::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_Operand));
}

ImplicitCastExpr::~ImplicitCastExpr()
{
}

AsTypeExpr::~AsTypeExpr()
{
}

BinaryOperator::~BinaryOperator()
{
}

StmtEnumerable BinaryOperator::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_LeftOperand), static_cast<StmtPtr>(m_RightOperand));
}

CompoundAssignOperator::~CompoundAssignOperator()
{
}

ConditionalOperator::~ConditionalOperator()
{
}

StmtEnumerable ConditionalOperator::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_Condition), static_cast<StmtPtr>(m_LeftOperand), static_cast<StmtPtr>(m_RightOperand));
}

StmtExpr::~StmtExpr()
{
}

StmtEnumerable StmtExpr::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_SubStmt));
}

ThisExpr::~ThisExpr()
{
}

ThrowExpr::~ThrowExpr()
{
}

StmtEnumerable ThrowExpr::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_Operand));
}

ConstructExpr::~ConstructExpr()
{
}

Linq<NatsuLang::Expression::ExprPtr> ConstructExpr::GetArgs() const noexcept
{
	return from(m_Args);
}

void ConstructExpr::SetArgs(Linq<ExprPtr> const& value)
{
	m_Args.assign(value.begin(), value.end());
}

StmtEnumerable ConstructExpr::GetChildrens()
{
	return from(m_Args).select([](ExprPtr const& expr) { return static_cast<StmtPtr>(expr); });
}

NewExpr::~NewExpr()
{
}

Linq<NatsuLang::Expression::ExprPtr> NewExpr::GetArgs() const noexcept
{
	return from(m_Args);
}

void NewExpr::SetArgs(Linq<ExprPtr> const& value)
{
	m_Args.assign(value.begin(), value.end());
}

StmtEnumerable NewExpr::GetChildrens()
{
	return from(m_Args).select([](ExprPtr const& expr) { return static_cast<StmtPtr>(expr); });
}

DeleteExpr::~DeleteExpr()
{
}

StmtEnumerable DeleteExpr::GetChildrens()
{
	return from_values(static_cast<StmtPtr>(m_Operand));
}
