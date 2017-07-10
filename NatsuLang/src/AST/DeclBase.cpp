#include "AST/DeclBase.h"

using namespace NatsuLang::Declaration;

namespace
{
	constexpr const char* getTypeName(Decl::Type type) noexcept
	{
		switch (type)
		{
#define DECL(Derived, Base) case Decl::Derived: return #Derived;
#define ABSTRACT_DECL(Decl)
#include "Basic/DeclDef.h"
		default:
			assert(!"Invalid type.");
			return "";
		}
	}
}

DeclContext* Decl::castToDeclContext(const Decl* decl)
{
	const auto type = decl->GetType();
	switch (type)
	{
#define DECL(Name, Base)
#define DECL_CONTEXT(Name) case Decl::Name: return static_cast<Name##Decl*>(const_cast<Decl*>(decl));
#define DECL_CONTEXT_BASE(Name)
#include "Basic/DeclDef.h"
	default:
#define DECL(Name, Base)
#define DECL_CONTEXT_BASE(Name) if (type >= First##Name && type <= Last##Name) return static_cast<Name##Decl*>(const_cast<DeclContext*>(decl));
#include "Basic/DeclDef.h"

		assert(!"Invalid type.");
		return nullptr;
	}
}

Decl* Decl::castFromDeclContext(const DeclContext* declContext)
{
	const auto type = declContext->GetType();
	switch (type)
	{
#define DECL(Name, Base)
#define DECL_CONTEXT(Name) case Decl::Name: return static_cast<Name##Decl*>(const_cast<DeclContext*>(declContext));
#define DECL_CONTEXT_BASE(Name)
#include "Basic/DeclDef.h"
	default:
#define DECL(Name, BASE)
#define DECL_CONTEXT_BASE(Name) if (type >= First##Name && type <= Last##Name) return static_cast<Name##Decl*>(const_cast<DeclContext*>(declContext));
#include "Basic/DeclDef.h"

		assert(!"Invalid type.");
		return nullptr;
	}
}

const char* Decl::GetTypeName() const noexcept
{
	return getTypeName(m_Type);
}

const char* DeclContext::GetTypeName() const noexcept
{
	return getTypeName(m_Type);
}
