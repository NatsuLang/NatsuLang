#pragma once
#include "Declaration.h"
#include "Statement.h"

namespace NatsuLang::Expression
{
	class Expr
		: public Statement::Stmt
	{
	public:
		Expr(StmtType stmtType, Type::TypePtr exprType);
		~Expr();

		Type::TypePtr GetExprType() const noexcept
		{
			return m_ExprType;
		}

		void SetExprType(Type::TypePtr value) noexcept
		{
			m_ExprType = std::move(value);
		}

		struct EvalStatus
		{
			nBool HasSideEffects;
			nBool HasUndefinedBehavior;
		};

	private:
		Type::TypePtr m_ExprType;
	};
}
