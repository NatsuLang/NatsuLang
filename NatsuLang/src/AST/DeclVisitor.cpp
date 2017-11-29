#include "AST/DeclVisitor.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

DeclVisitor::~DeclVisitor()
{
}

void DeclVisitor::Visit(Declaration::DeclPtr const& decl)
{
	decl->Accept(natRefPointer<DeclVisitor>{ this });
}

#define DECL(Type, Base) void DeclVisitor::Visit##Type##Decl(NatsuLib::natRefPointer<Declaration::Type##Decl> const& decl) { Visit##Base(decl); }
#include "Basic/DeclDef.h"

void DeclVisitor::VisitDecl(Declaration::DeclPtr const& /*decl*/)
{
}
