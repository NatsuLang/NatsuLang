#include "AST/Declaration.h"
#include "Basic/Identifier.h"

using namespace NatsuLang::Declaration;

NamedDecl::~NamedDecl()
{
}

nStrView NamedDecl::GetName() const noexcept
{
	return m_IdentifierInfo->GetName();
}
