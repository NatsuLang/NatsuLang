#include "AST/Type.h"
#include "Basic/Token.h"

using namespace NatsuLib;
using namespace NatsuLang::Type;

namespace
{
	constexpr const char* GetBuiltinTypeName(BuiltinType::BuiltinClass builtinClass) noexcept
	{
		switch (builtinClass)
		{
#define BUILTIN_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return #Name;
#include "Basic/BuiltinTypesDef.h"
		default:
			assert(!"Invalid BuiltinClass.");
			return "";
		}
	}

	constexpr nBool IsBuiltinTypeInteger(BuiltinType::BuiltinClass builtinClass) noexcept
	{
		switch (builtinClass)
		{
#define BUILTIN_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#define SIGNED_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return true;
#define UNSIGNED_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return true;
#define FLOATING_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#define PLACEHOLDER_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#include "Basic/BuiltinTypesDef.h"
		default:
			assert(!"Invalid BuiltinClass.");
			return false;
		}
	}

	constexpr nBool IsBuiltinTypeFloating(BuiltinType::BuiltinClass builtinClass) noexcept
	{
		switch (builtinClass)
		{
#define BUILTIN_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#define SIGNED_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#define UNSIGNED_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#define FLOATING_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return true;
#define PLACEHOLDER_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#include "Basic/BuiltinTypesDef.h"
		default:
			assert(!"Invalid BuiltinClass.");
			return false;
		}
	}
}

BuiltinType::~BuiltinType()
{
}

const char* BuiltinType::GetName() const noexcept
{
	return GetBuiltinTypeName(m_BuiltinClass);
}

std::size_t BuiltinType::GetHashCode() const noexcept
{
	return std::hash<BuiltinClass>{}(m_BuiltinClass);
}

nBool BuiltinType::EqualTo(TypePtr const& other) const noexcept
{
	const auto realOther = static_cast<natRefPointer<BuiltinType>>(other);
	if (!realOther)
	{
		return false;
	}

	return m_BuiltinClass == realOther->m_BuiltinClass;
}

BuiltinType::BuiltinClass BuiltinType::GetBuiltinClassFromTokenType(Token::TokenType type) noexcept
{
	switch (type)
	{
	case Token::TokenType::Kw_bool:
		return Bool;
	case Token::TokenType::Kw_char:
		return Char;
	case Token::TokenType::Kw_ushort:
		return UShort;
	case Token::TokenType::Kw_uint:
		return UInt;
	case Token::TokenType::Kw_ulong:
		return ULong;
	case Token::TokenType::Kw_ulonglong:
		return ULongLong;
	case Token::TokenType::Kw_uint128:
		return UInt128;
	case Token::TokenType::Kw_short:
		return Short;
	case Token::TokenType::Kw_int:
		return Int;
	case Token::TokenType::Kw_long:
		return Long;
	case Token::TokenType::Kw_longlong:
		return LongLong;
	case Token::TokenType::Kw_int128:
		return Int128;
	default:
		return Invalid;
	}
}

nBool BuiltinType::IsIntegerBuiltinClass(BuiltinClass builtinClass) noexcept
{
	return IsBuiltinTypeInteger(builtinClass);
}

nBool BuiltinType::IsFloatingBuiltinClass(BuiltinClass builtinClass) noexcept
{
	return IsBuiltinTypeFloating(builtinClass);
}

ParenType::~ParenType()
{
}

std::size_t ParenType::GetHashCode() const noexcept
{
	return m_InnerType->GetHashCode();
}

nBool ParenType::EqualTo(TypePtr const& other) const noexcept
{
	const auto realOther = static_cast<natRefPointer<ParenType>>(other);
	if (!realOther)
	{
		return false;
	}

	return m_InnerType == realOther->m_InnerType;
}

ArrayType::~ArrayType()
{
}

std::size_t ArrayType::GetHashCode() const noexcept
{
	return std::hash<std::size_t>{}(m_ArraySize) ^ m_ElementType->GetHashCode();
}

nBool ArrayType::EqualTo(TypePtr const& other) const noexcept
{
	const auto realOther = static_cast<natRefPointer<ArrayType>>(other);
	if (!realOther)
	{
		return false;
	}

	return m_ArraySize == realOther->m_ArraySize && m_ElementType->EqualTo(realOther->m_ElementType);
}

FunctionType::~FunctionType()
{
}

Linq<TypePtr> FunctionType::GetParameterTypes() const noexcept
{
	return from(m_ParameterTypes);
}

std::size_t FunctionType::GetParameterCount() const noexcept
{
	return m_ParameterTypes.size();
}

std::size_t FunctionType::GetHashCode() const noexcept
{
	return from(m_ParameterTypes).select([](TypePtr const& type)
	{
		return type->GetHashCode();
	}).aggregate(m_ResultType->GetHashCode(), [](std::size_t ret, std::size_t cur)
	{
		return ret ^ cur;
	});
}

nBool FunctionType::EqualTo(TypePtr const& other) const noexcept
{
	const auto realOther = static_cast<natRefPointer<FunctionType>>(other);
	if (!realOther)
	{
		return false;
	}

	return m_ResultType->EqualTo(realOther->m_ResultType) &&
		from(m_ParameterTypes)
		.zip(from(realOther->m_ParameterTypes))
		.all([](std::pair<const TypePtr, const TypePtr> const& typePair)
		{
			return typePair.first->EqualTo(typePair.second);
		});
}

TypeOfType::~TypeOfType()
{
}

std::size_t TypeOfType::GetHashCode() const noexcept
{
	return std::hash<natRefPointer<Expression::Expr>>{}(m_Expr) ^ m_UnderlyingType->GetHashCode();
}

nBool TypeOfType::EqualTo(TypePtr const& other) const noexcept
{
	const auto realOther = static_cast<natRefPointer<TypeOfType>>(other);
	if (!realOther)
	{
		return false;
	}

	return m_UnderlyingType == realOther->m_UnderlyingType && m_Expr == realOther->m_Expr;
}

TagType::~TagType()
{
}

std::size_t TagType::GetHashCode() const noexcept
{
	return std::hash<natRefPointer<Declaration::TagDecl>>{}(m_Decl);
}

nBool TagType::EqualTo(TypePtr const& other) const noexcept
{
	const auto realOther = static_cast<natRefPointer<TagType>>(other);
	if (!realOther)
	{
		return false;
	}

	return m_Decl == realOther->m_Decl;
}

RecordType::~RecordType()
{
}

nBool RecordType::EqualTo(TypePtr const& other) const noexcept
{
	if (other->GetType() != Record)
	{
		return false;
	}

	return static_cast<TagType const&>(*this).EqualTo(other);
}

EnumType::~EnumType()
{
}

nBool EnumType::EqualTo(TypePtr const& other) const noexcept
{
	if (other->GetType() != Enum)
	{
		return false;
	}

	return static_cast<TagType const&>(*this).EqualTo(other);
}

DeducedType::~DeducedType()
{
}

std::size_t DeducedType::GetHashCode() const noexcept
{
	return m_DeducedAsType->GetHashCode();
}

nBool DeducedType::EqualTo(TypePtr const& other) const noexcept
{
	const auto realOther = static_cast<natRefPointer<DeducedType>>(other);
	if (!realOther)
	{
		return false;
	}

	return m_DeducedAsType->EqualTo(realOther->m_DeducedAsType);
}

AutoType::~AutoType()
{
}

nBool AutoType::EqualTo(TypePtr const& other) const noexcept
{
	if (other->GetType() != Auto)
	{
		return false;
	}

	return static_cast<DeducedType const&>(*this).EqualTo(other);
}
