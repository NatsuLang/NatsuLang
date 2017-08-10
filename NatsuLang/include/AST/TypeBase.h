#pragma once
#include <natRefObj.h>

namespace NatsuLang::Token
{
	enum class TokenType;
}

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

		virtual std::size_t GetHashCode() const noexcept = 0;
		virtual nBool EqualTo(NatsuLib::natRefPointer<Type> const& other) const noexcept = 0;

	private:
		const TypeClass m_TypeClass;
	};

	using TypePtr = NatsuLib::natRefPointer<Type>;

	struct TypeHash
	{
		std::size_t operator()(TypePtr const& type) const noexcept
		{
			return type->GetHashCode();
		}
	};

	struct TypeEqualTo
	{
		nBool operator()(TypePtr const& a, TypePtr const& b) const
		{
			return a->EqualTo(b);
		}
	};
}
