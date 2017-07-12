#pragma once
#include <natRefObj.h>

namespace NatsuLang::Type
{
#define TYPE(Class, Base) class Class##Type;
#include "Basic/TypeDef.h"

	class Type
		: public NatsuLib::natRefObjImpl<Type>
	{
	public:
		enum TypeClass
		{
#define TYPE(Class, Base) Class,
#define LAST_TYPE(Class) TypeLast = Class,
#define ABSTRACT_TYPE(Class, Base)
#include "Basic/TypeDef.h"
			TagFirst = Record, TagLast = Enum
		};

		explicit Type(TypeClass typeClass)
			: m_TypeClass{ typeClass }
		{
		}

		~Type();

		TypeClass GetType() const noexcept
		{
			return m_TypeClass;
		}

	private:
		const TypeClass m_TypeClass;
	};

	using TypePtr = NatsuLib::natRefPointer<Type>;

	class BuiltinType
		: public Type
	{
	public:
		enum BuiltinClass
		{
#define BUILTIN_TYPE(Id, SingletonId, Name) Id,
#define LAST_BUILTIN_TYPE(Id) LastKind = Id
#include "Basic/BuiltinTypesDef.h"
		};

		explicit BuiltinType(BuiltinClass builtinClass)
			: Type{ Builtin }, m_BuiltinClass{ builtinClass }
		{
		}

		~BuiltinType();

		BuiltinClass GetBuiltinClass() const noexcept
		{
			return m_BuiltinClass;
		}

		const char* GetName() const noexcept;

	private:
		const BuiltinClass m_BuiltinClass;
	};

}
