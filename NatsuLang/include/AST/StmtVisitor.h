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
			
		}


	};

	template <typename RetType>
	StmtVisitorBase<RetType>::~StmtVisitorBase()
	{
	}
}
