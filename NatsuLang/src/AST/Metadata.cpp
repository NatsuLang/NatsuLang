#include "AST/Metadata.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

Metadata::Metadata()
{
}

Metadata::~Metadata()
{
}

void Metadata::AddDecl(Declaration::DeclPtr decl)
{
	if (decl)
	{
		m_Decls.emplace_back(std::move(decl));
	}
}

std::size_t Metadata::GetDeclCount() const noexcept
{
	return m_Decls.size();
}

Linq<Valued<Declaration::DeclPtr>> Metadata::GetDecls() const
{
	return from(m_Decls);
}
