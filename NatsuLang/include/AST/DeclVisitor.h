#pragma once
#include <natRefObj.h>

namespace NatsuLang
{
	namespace Declaration
	{
		class Decl;
		using DeclPtr = NatsuLib::natRefPointer<Decl>;

#define DECL(Type, Base) class Type##Decl;
#include "Basic/DeclDef.h"
	}

	struct DeclVisitor
		: NatsuLib::natRefObj
	{
		virtual ~DeclVisitor() = 0;

		virtual void Visit(Declaration::DeclPtr const& decl);

#define DECL(Type, Base) virtual void Visit##Type##Decl(NatsuLib::natRefPointer<Declaration::Type##Decl> const& decl);
#include "Basic/DeclDef.h"

		virtual void VisitDecl(Declaration::DeclPtr const& decl);
	};
}
