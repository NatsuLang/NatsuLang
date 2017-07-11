#include "AST/Statement.h"
#include "AST/Declaration.h"
#include "AST/Expression.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang::Statement;

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

Stmt::StmtEnumerable CompoundStmt::GetChildrens()
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

Stmt::StmtEnumerable SwitchStmt::GetChildrens()
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

Stmt::StmtEnumerable LabelStmt::GetChildrens()
{
	return from_values({ m_SubStmt });
}

IfStmt::~IfStmt()
{
}

Stmt::StmtEnumerable IfStmt::GetChildrens()
{
	return from_values({ static_cast<StmtPtr>(m_Cond), m_Then, m_Else });
}

WhileStmt::~WhileStmt()
{
}

Stmt::StmtEnumerable WhileStmt::GetChildrens()
{
	return from_values({ static_cast<StmtPtr>(m_Cond), m_Body });
}

DoStmt::~DoStmt()
{
}

Stmt::StmtEnumerable DoStmt::GetChildrens()
{
	return from_values({ m_Body, static_cast<StmtPtr>(m_Cond) });
}

ForStmt::~ForStmt()
{
}

Stmt::StmtEnumerable ForStmt::GetChildrens()
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
	: Stmt{ ReturnStmtClass, loc, retExpr ? retExpr->GetEndLoc() : loc }
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

Stmt::StmtEnumerable ReturnStmt::GetChildrens()
{
	if (m_RetExpr)
	{
		return from_values({ static_cast<StmtPtr>(m_RetExpr) });
	}
	
	return Stmt::GetChildrens();
}
