#include "AST/StmtVisitor.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

StmtVisitor::~StmtVisitor()
{
}

void StmtVisitor::Visit(natRefPointer<Statement::Stmt> const& stmt)
{
	stmt->Accept(natRefPointer<StmtVisitor>{ this });
}

#define STMT(Type, Base) void StmtVisitor::Visit##Type(natRefPointer<Statement::Type> const& stmt) { Visit##Base(stmt); }
#define EXPR(Type, Base) void StmtVisitor::Visit##Type(natRefPointer<Expression::Type> const& expr) { Visit##Base(expr); }
#include "Basic/StmtDef.h"

void StmtVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
}
