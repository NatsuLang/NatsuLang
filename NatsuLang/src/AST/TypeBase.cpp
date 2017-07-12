#include "AST/TypeBase.h"

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
