#pragma once
#include <natRefObj.h>

namespace NatsuLang
{
	namespace Type
	{
		class Type;
		using TypePtr = NatsuLib::natRefPointer<Type>;

#define TYPE(Class, Base) class Class##Type;
#include "Basic/TypeDef.h"
	}

	struct TypeVisitor
		: NatsuLib::natRefObjImpl<TypeVisitor>
	{
		virtual ~TypeVisitor() = 0;

		virtual void Visit(Type::TypePtr const& type);

#define TYPE(Class, Base) virtual void Visit##Class##Type(NatsuLib::natRefPointer<Type::Class##Type> const& type);
#include "Basic/TypeDef.h"

		virtual void VisitType(Type::TypePtr const& type);
	};
}
