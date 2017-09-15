#pragma once
#include "Statement.h"
#include "Expression.h"

namespace NatsuLang
{
	template <typename RetType>
	struct StmtVisitorBase
	{
		virtual ~StmtVisitorBase();

		RetType Visit(NatsuLib::natRefPointer<Statement::Stmt> const& stmt)
		{
			switch (stmt->GetType())
			{
#define STMT(Type, Base) case Statement::Stmt::Type##Class: return Visit##Type(stmt);
#define ABSTRACT_STMT(Type)
#include "Basic/StmtDef.h"
			default:
				// TODO: 修改为优雅的实现
				if (const auto castExpr = static_cast<NatsuLib::natRefPointer<Expression::CastExpr>>(stmt))
				{
					return VisitCastExpr(castExpr);
				}

				if (const auto expr = static_cast<NatsuLib::natRefPointer<Expression::Expr>>(stmt))
				{
					return VisitExpr(expr);
				}

				return VisitStmt(stmt);
			}
		}

#define STMT(Type, Base) virtual RetType Visit##Type(NatsuLib::natRefPointer<Statement::Type> const& stmt) { return Visit##Base(stmt); }
#define EXPR(Type, Base) virtual RetType Visit##Type(NatsuLib::natRefPointer<Expression::Type> const& expr) { return Visit##Base(expr); }
#include "Basic/StmtDef.h"

		virtual RetType VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt) = 0;
	};

	template <typename RetType>
	StmtVisitorBase<RetType>::~StmtVisitorBase()
	{
	}
}
