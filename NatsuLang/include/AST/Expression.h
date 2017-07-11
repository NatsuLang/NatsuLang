#pragma once
#include "Declaration.h"
#include "Statement.h"

namespace NatsuLang::Expression
{
	class Expr
		: public Statement::Stmt
	{
	public:
		Expr();
		~Expr();

	private:

	};
}
