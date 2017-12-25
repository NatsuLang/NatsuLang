#include "AST/Type.h"
#include "AST/Expression.h"
#include "Basic/Token.h"
#include "Basic/Identifier.h"
#include "AST/NestedNameSpecifier.h"
#include "AST/TypeVisitor.h"

using namespace NatsuLib;
using namespace NatsuLang;
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

	constexpr nBool IsBuiltinTypeSigned(BuiltinType::BuiltinClass builtinClass) noexcept
	{
		switch (builtinClass)
		{
#define BUILTIN_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
#define SIGNED_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return true;
#define UNSIGNED_TYPE(Id, Name) case BuiltinType::BuiltinClass::Id: return false;
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

	constexpr BuiltinType::BuiltinClass MakeBuiltinTypeUnsigned(BuiltinType::BuiltinClass builtinClass) noexcept
	{
		switch (builtinClass)
		{
		case BuiltinType::Short:
			return BuiltinType::UShort;
		case BuiltinType::Int:
			return BuiltinType::UInt;
		case BuiltinType::Long:
			return BuiltinType::ULong;
		case BuiltinType::LongLong:
			return BuiltinType::ULongLong;
		case BuiltinType::Int128:
			return BuiltinType::UInt128;
		default:
			return builtinClass;
		}
	}

	constexpr BuiltinType::BuiltinClass MakeBuiltinTypeSigned(BuiltinType::BuiltinClass builtinClass) noexcept
	{
		switch (builtinClass)
		{
		case BuiltinType::UShort:
			return BuiltinType::Short;
		case BuiltinType::UInt:
			return BuiltinType::Int;
		case BuiltinType::ULong:
			return BuiltinType::Long;
		case BuiltinType::ULongLong:
			return BuiltinType::LongLong;
		case BuiltinType::UInt128:
			return BuiltinType::Int128;
		default:
			return builtinClass;
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
	const auto realOther = other.Cast<BuiltinType>();
	if (!realOther)
	{
		return false;
	}

	return m_BuiltinClass == realOther->m_BuiltinClass;
}

BuiltinType::BuiltinClass BuiltinType::GetBuiltinClassFromTokenType(Lex::TokenType type) noexcept
{
	switch (type)
	{
#define BUILTIN_TYPE(Id, Name) case Lex::TokenType::Kw_ ## Name: return Id;
#define SIGNED_TYPE(Id, Name) case Lex::TokenType::Kw_ ## Name: return Id;
#define UNSIGNED_TYPE(Id, Name) case Lex::TokenType::Kw_ ## Name: return Id;
#define FLOATING_TYPE(Id, Name) case Lex::TokenType::Kw_ ## Name: return Id;
#define PLACEHOLDER_TYPE(Id, Name)
#include "Basic/BuiltinTypesDef.h"
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

nBool BuiltinType::IsSignedBuiltinClass(BuiltinClass builtinClass) noexcept
{
	return IsBuiltinTypeSigned(builtinClass);
}

BuiltinType::BuiltinClass BuiltinType::MakeSignedBuiltinClass(BuiltinClass builtinClass) noexcept
{
	return MakeBuiltinTypeSigned(builtinClass);
}

BuiltinType::BuiltinClass BuiltinType::MakeUnsignedBuiltinClass(BuiltinClass builtinClass) noexcept
{
	return MakeBuiltinTypeUnsigned(builtinClass);
}

nBool BuiltinType::CompareRankTo(natRefPointer<BuiltinType> const& other, nInt& result) const noexcept
{
	if (IsFloatingType())
	{
		if (!other->IsFloatingType())
		{
			return false;
		}

		result = m_BuiltinClass - other->GetBuiltinClass();
		return true;
	}

	if (IsIntegerType())
	{
		if (!other->IsIntegerType())
		{
			return false;
		}

		result = MakeBuiltinTypeSigned(m_BuiltinClass) - MakeBuiltinTypeSigned(other->GetBuiltinClass());
		return true;
	}
	
	return false;
}

PointerType::~PointerType()
{
}

std::size_t PointerType::GetHashCode() const noexcept
{
	return m_PointeeType->GetHashCode();
}

nBool PointerType::EqualTo(TypePtr const& other) const noexcept
{
	if (const auto realOther = other.Cast<PointerType>())
	{
		return m_PointeeType == realOther->m_PointeeType;
	}

	return false;
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
	const auto realOther = other.Cast<ParenType>();
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
	const auto realOther = other.Cast<ArrayType>();
	if (!realOther)
	{
		return false;
	}

	return m_ArraySize == realOther->m_ArraySize && m_ElementType->EqualTo(realOther->m_ElementType);
}

FunctionType::~FunctionType()
{
}

Linq<NatsuLib::Valued<TypePtr>> FunctionType::GetParameterTypes() const noexcept
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
	const auto realOther = other.Cast<FunctionType>();
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
	const auto realOther = other.Cast<TypeOfType>();
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
	const auto realOther = other.Cast<TagType>();
	if (!realOther)
	{
		return false;
	}

	return m_Decl == realOther->m_Decl;
}

ClassType::ClassType(natRefPointer<Declaration::ClassDecl> recordDecl)
	: TagType{Class, recordDecl}
{
}

ClassType::ClassType(TypeClass typeClass, NatsuLib::natRefPointer<Declaration::ClassDecl> recordDecl)
	: TagType{ typeClass, recordDecl }
{
}

ClassType::~ClassType()
{
}

nBool ClassType::EqualTo(TypePtr const& other) const noexcept
{
	if (other->GetType() != Class)
	{
		return false;
	}

	return static_cast<TagType const&>(*this).EqualTo(other);
}

EnumType::EnumType(natRefPointer<Declaration::EnumDecl> decl)
	: TagType{ Enum, decl }
{
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
	const auto realOther = other.Cast<DeducedType>();
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

UnresolvedType::~UnresolvedType()
{
}

std::size_t UnresolvedType::GetHashCode() const noexcept
{
	return from(m_Tokens).aggregate(std::size_t{}, [](std::size_t result, Lex::Token const& token)
	{
		result ^= static_cast<std::size_t>(token.GetType());
		if (token.Is(Lex::TokenType::Identifier))
		{
			result ^= std::hash<Identifier::IdPtr>{}(token.GetIdentifierInfo());
		}
		return result;
	});
}

nBool UnresolvedType::EqualTo(TypePtr const& other) const noexcept
{
	if (const auto unresolvedOther = other.Cast<UnresolvedType>())
	{
		return m_Tokens == unresolvedOther->m_Tokens;
	}

	return false;
}

#define TYPE(Class, Base) void Class##Type::Accept(NatsuLib::natRefPointer<TypeVisitor> const& visitor) { visitor->Visit##Class##Type(ForkRef<Class##Type>()); }
#include "Basic/TypeDef.h"
