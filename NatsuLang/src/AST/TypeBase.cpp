#include "AST/TypeBase.h"
#include "Basic/Token.h"

using namespace NatsuLib;
using namespace NatsuLang::Type;

namespace
{
	constexpr const char* GetBuiltinTypeName(BuiltinType::BuiltinClass builtinClass) noexcept
	{
		switch (builtinClass)
		{
#define BUILTIN_TYPE(Id, SingletonId, Name) case BuiltinType::BuiltinClass::Id: return #Name;
#include "Basic/BuiltinTypesDef.h"
		default:
			assert(!"Invalid BuiltinClass.");
			return "";
		}
	}
}

Type::~Type()
{
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

BuiltinType::BuiltinClass BuiltinType::GetBuiltinClassFromTokenType(Token::TokenType type)
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
