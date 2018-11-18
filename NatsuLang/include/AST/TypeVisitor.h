#pragma once
#include "Type.h"

namespace NatsuLang
{
	template <typename T, typename ReturnType = void>
	struct TypeVisitor
	{
		ReturnType Visit(Type::TypePtr const& type)
		{
			switch (type->GetType())
			{
#define TYPE(Class, Base) case Type::Type::Class: return static_cast<T*>(this)->Visit##Class##Type(type.UnsafeCast<Type::Class##Type>());
#define ABSTRACT_TYPE(Class, Base)
#include "Basic/TypeDef.h"
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

#define TYPE(Class, Base) ReturnType Visit##Class##Type(NatsuLib::natRefPointer<Type::Class##Type> const& type) { return static_cast<T*>(this)->Visit##Base(type); }
#include "Basic/TypeDef.h"

		ReturnType VisitType(Type::TypePtr const& type)
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
		~TypeVisitor() = default;
	};
}
