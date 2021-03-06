﻿#include "AST/Statement.h"
#include "AST/Declaration.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Statement;
using namespace NatsuLang::Expression;

DeclStmt::~DeclStmt()
{
}

NullStmt::~NullStmt()
{
}

void NullStmt::SetStartLoc(SourceLocation loc) noexcept
{
	Stmt::SetStartLoc(loc);
	Stmt::SetEndLoc(loc);
}

void NullStmt::SetEndLoc(SourceLocation loc) noexcept
{
	Stmt::SetStartLoc(loc);
	Stmt::SetEndLoc(loc);
}

CompoundStmt::~CompoundStmt()
{
}

StmtEnumerable CompoundStmt::GetChildrenStmt()
{
	return from(m_Stmts);
}

void CompoundStmt::SetStmts(StmtEnumerable const& stmts)
{
	m_Stmts.assign(std::cbegin(stmts), std::cend(stmts));
}

SwitchCase::~SwitchCase()
{
}

SwitchStmt::~SwitchStmt()
{
}

StmtEnumerable SwitchStmt::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Cond), m_Body });
}

CaseStmt::~CaseStmt()
{
}

DefaultStmt::~DefaultStmt()
{
}

LabelStmt::~LabelStmt()
{
}

nStrView LabelStmt::GetName() const noexcept
{
	return m_Decl.Lock()->GetIdentifierInfo()->GetName();
}

StmtEnumerable LabelStmt::GetChildrenStmt()
{
	return from_values({ m_SubStmt });
}

IfStmt::~IfStmt()
{
}

StmtEnumerable IfStmt::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Cond), m_Then, m_Else });
}

WhileStmt::~WhileStmt()
{
}

StmtEnumerable WhileStmt::GetChildrenStmt()
{
	return from_values({ static_cast<StmtPtr>(m_Cond), m_Body });
}

DoStmt::~DoStmt()
{
}

StmtEnumerable DoStmt::GetChildrenStmt()
{
	return from_values({ m_Body, static_cast<StmtPtr>(m_Cond) });
}

ForStmt::~ForStmt()
{
}

StmtEnumerable ForStmt::GetChildrenStmt()
{
	return from_values({ m_Init, static_cast<StmtPtr>(m_Cond),  static_cast<StmtPtr>(m_Inc), m_Body });
}

GotoStmt::~GotoStmt()
{
}

ContinueStmt::~ContinueStmt()
{
}

BreakStmt::~BreakStmt()
{
}

ReturnStmt::ReturnStmt(SourceLocation loc, ExprPtr retExpr)
	: Stmt{ ReturnStmtClass, loc, retExpr ? retExpr->GetEndLoc() : loc }, m_RetExpr{ std::move(retExpr) }
{
}

ReturnStmt::~ReturnStmt()
{
}

void ReturnStmt::SetReturnExpr(ExprPtr value) noexcept
{
	m_RetExpr = std::move(value);
	SetEndLoc(m_RetExpr ? m_RetExpr->GetEndLoc() : GetStartLoc());
}

StmtEnumerable ReturnStmt::GetChildrenStmt()
{
	if (m_RetExpr)
	{
		return from_values({ static_cast<StmtPtr>(m_RetExpr) });
	}

	return Stmt::GetChildrenStmt();
}

TryStmt::~TryStmt()
{
}

CatchStmt::~CatchStmt()
{
}
