#include "AST/Statement.h"
#include "AST/Declaration.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "AST/StmtVisitor.h"
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

StmtEnumerable CompoundStmt::GetChildrens()
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

StmtEnumerable SwitchStmt::GetChildrens()
{
	return from_values(std::vector<StmtPtr>{ static_cast<StmtPtr>(m_Cond), m_Body });
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

StmtEnumerable LabelStmt::GetChildrens()
{
	return from_values(std::vector<StmtPtr>{ m_SubStmt });
}

IfStmt::~IfStmt()
{
}

StmtEnumerable IfStmt::GetChildrens()
{
	return from_values(std::vector<StmtPtr>{ static_cast<StmtPtr>(m_Cond), m_Then, m_Else });
}

WhileStmt::~WhileStmt()
{
}

StmtEnumerable WhileStmt::GetChildrens()
{
	return from_values(std::vector<StmtPtr>{ static_cast<StmtPtr>(m_Cond), m_Body });
}

DoStmt::~DoStmt()
{
}

StmtEnumerable DoStmt::GetChildrens()
{
	return from_values(std::vector<StmtPtr>{ m_Body, static_cast<StmtPtr>(m_Cond) });
}

ForStmt::~ForStmt()
{
}

StmtEnumerable ForStmt::GetChildrens()
{
	return from_values(std::vector<StmtPtr>{ m_Init, static_cast<StmtPtr>(m_Cond),  static_cast<StmtPtr>(m_Inc), m_Body });
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

StmtEnumerable ReturnStmt::GetChildrens()
{
	if (m_RetExpr)
	{
		return from_values(std::vector<StmtPtr>{ static_cast<StmtPtr>(m_RetExpr) });
	}
	
	return Stmt::GetChildrens();
}

TryStmt::~TryStmt()
{
}

CatchStmt::~CatchStmt()
{
}

#define DEFAULT_ACCEPT_DEF(Type) void Type::Accept(NatsuLib::natRefPointer<StmtVisitor> const& visitor) { visitor->Visit##Type(ForkRef<Type>()); }
#define STMT(StmtType, Base) DEFAULT_ACCEPT_DEF(StmtType)
#define EXPR(ExprType, Base)
#include "Basic/StmtDef.h"
