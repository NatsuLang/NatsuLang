#pragma once
#include "Expression.h"

namespace NatsuLang
{
	template <typename T, typename ReturnType = void>
	struct StmtVisitor
	{
		ReturnType Visit(Statement::StmtPtr const& stmt)
		{
			switch (stmt->GetType())
			{
#define STMT(Type, Base) case Statement::Stmt::Type##Class: return static_cast<T*>(this)->Visit##Type(stmt.UnsafeCast<Statement::Type>());
#define EXPR(Type, Base) case Statement::Stmt::Type##Class: return static_cast<T*>(this)->Visit##Type(stmt.UnsafeCast<Expression::Type>());
#define ABSTRACT_STMT(Stmt)
#include "Basic/StmtDef.h"
			default:
				assert(!"Invalid StmtType.");
				if constexpr (std::is_void_v<ReturnType>)
				{
					return;
				}
				else
				{
					return ReturnType{};
				}
			}
		}

#define STMT(Type, Base) ReturnType Visit##Type(NatsuLib::natRefPointer<Statement::Type> const& stmt) { return static_cast<T*>(this)->Visit##Base(stmt); }
#define EXPR(Type, Base) ReturnType Visit##Type(NatsuLib::natRefPointer<Expression::Type> const& expr)  { return static_cast<T*>(this)->Visit##Base(expr); }
#include "Basic/StmtDef.h"

		ReturnType VisitStmt(Statement::StmtPtr const& stmt)
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				return;
			}
			else
			{
				return ReturnType{};
			}
		}

	protected:
		~StmtVisitor() = default;
	};
}
