#include "AST/Type.h"

using namespace NatsuLib;
using namespace NatsuLang::Type;

ParenType::~ParenType()
{
}

ArrayType::~ArrayType()
{
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

TypeOfType::~TypeOfType()
{
}

TagType::~TagType()
{
}

RecordType::~RecordType()
{
}

EnumType::~EnumType()
{
}

DeducedType::~DeducedType()
{
}

AutoType::~AutoType()
{
}
