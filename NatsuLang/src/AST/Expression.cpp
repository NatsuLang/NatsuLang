#include "AST/Expression.h"

using namespace NatsuLang::Statement;
using namespace NatsuLang::Expression;

Expr::Expr(StmtType stmtType, Type::TypePtr exprType)
	: Stmt{ stmtType }, m_ExprType{ std::move(exprType) }
{
}

Expr::~Expr()
{
}
