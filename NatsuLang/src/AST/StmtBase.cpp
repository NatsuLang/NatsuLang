#include "AST/StmtBase.h"
#include "AST/StmtVisitor.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Statement;

namespace
{
	constexpr const char* getStmtTypeName(Stmt::StmtType type) noexcept
	{
		switch (type)
		{
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT) case Stmt::CLASS##Class: return #CLASS;
#include "Basic/StmtDef.h"
		default:
			assert(!"Invalid type.");
			return "";
		}
	}
}

Stmt::~Stmt()
{
}

const char* Stmt::GetTypeName() const noexcept
{
	return getStmtTypeName(m_Type);
}

StmtEnumerable Stmt::GetChildrenStmt()
{
	return NatsuLib::from_empty<StmtPtr>();
}

NatsuLang::SourceLocation Stmt::GetStartLoc() const noexcept
{
	return m_Start;
}

void Stmt::SetStartLoc(SourceLocation loc) noexcept
{
	m_Start = loc;
}

NatsuLang::SourceLocation Stmt::GetEndLoc() const noexcept
{
	return m_End;
}

void Stmt::SetEndLoc(SourceLocation loc) noexcept
{
	m_End = loc;
}

void Stmt::Accept(natRefPointer<StmtVisitor> const& visitor)
{
	visitor->VisitStmt(ForkRef());
}
