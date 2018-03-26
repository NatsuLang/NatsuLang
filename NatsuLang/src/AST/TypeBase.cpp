#include "AST/TypeBase.h"
#include "AST/Type.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Token.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Type;

NatsuLang::Type::Type::~Type()
{
}

nBool NatsuLang::Type::Type::IsVoid() const noexcept
{
	if (m_TypeClass != Builtin)
	{
		return false;
	}

	return static_cast<const BuiltinType*>(this)->GetBuiltinClass() == BuiltinType::Void;
}

TypePtr NatsuLang::Type::Type::GetUnderlyingType(TypePtr const& type)
{
	if (!type)
	{
		return nullptr;
	}

	if (const auto deducedType = type.Cast<DeducedType>())
	{
		return GetUnderlyingType(deducedType->GetDeducedAsType());
	}

	if (const auto parenType = type.Cast<ParenType>())
	{
		return GetUnderlyingType(parenType->GetInnerType());
	}

	return type;
}
