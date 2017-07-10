#include "AST/Statement.h"

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

Linq<Stmt> CompoundStmt::GetChildrens()
{
	return from(m_Stmts).select([](Stmt* ptr) { return *ptr; });
}

void CompoundStmt::SetStmts(Container<Stmt*> const& stmts)
{
	m_Stmts.clear();
	if (stmts.HasContainer())
	{
		m_Stmts.insert(m_Stmts.end(), std::cbegin(stmts), std::cend(stmts));
	}
}

SwitchCase::~SwitchCase()
{
}

CaseStmt::~CaseStmt()
{
}

DefaultStmt::~DefaultStmt()
{
}
