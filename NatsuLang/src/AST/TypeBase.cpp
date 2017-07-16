#include "AST/TypeBase.h"

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
