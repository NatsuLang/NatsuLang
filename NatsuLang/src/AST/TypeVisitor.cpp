#include "AST/TypeVisitor.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

TypeVisitor::~TypeVisitor()
{
}

void TypeVisitor::Visit(Type::TypePtr const& type)
{
	type->Accept(natRefPointer<TypeVisitor>{ this });
}

#define TYPE(Class, Base) void TypeVisitor::Visit##Class##Type(NatsuLib::natRefPointer<Type::Class##Type> const& type) { Visit##Base(type); }
#include "Basic/TypeDef.h"

void TypeVisitor::VisitType(Type::TypePtr const& /*type*/)
{
}
