#pragma once
#include "Declaration.h"

namespace NatsuLang
{
	template <typename T, typename ReturnType = void>
	struct DeclVisitor
	{
		ReturnType Visit(Declaration::DeclPtr const& decl)
		{
			switch (decl->GetType())
			{
#define DECL(Type, Base) case Declaration::Decl::Type: return static_cast<T*>(this)->Visit##Type##Decl(decl.UnsafeCast<Declaration::Type##Decl>());
#define ABSTRACT_DECL(Decl)
#include "Basic/DeclDef.h"
			default:
				assert(!"Invalid StmtType.");
				if constexpr (std::is_void_v<ReturnType>)
				{
					return;
				}
				else
				{
					return ReturnType{};
				}
			}
		}

#define DECL(Type, Base) ReturnType Visit##Type##Decl(NatsuLib::natRefPointer<Declaration::Type##Decl> const& decl) { return static_cast<T*>(this)->Visit##Base(decl); }
#include "Basic/DeclDef.h"

		ReturnType VisitDecl(Declaration::DeclPtr const& decl)
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				return;
			}
			else
			{
				return ReturnType{};
			}
		}

	protected:
		~DeclVisitor() = default;
	};
}
