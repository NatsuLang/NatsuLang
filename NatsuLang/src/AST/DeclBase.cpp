#include "AST/DeclBase.h"
#include "AST/Declaration.h"

using namespace NatsuLib;
using namespace NatsuLang::Declaration;

namespace
{
	constexpr const char* getTypeName(Decl::DeclType type) noexcept
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

DeclContext* Decl::CastToDeclContext(const Decl* decl)
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
#define DECL_CONTEXT_BASE(Name) if (type >= First##Name && type <= Last##Name) return static_cast<Name##Decl*>(const_cast<Decl*>(decl));
#include "Basic/DeclDef.h"

		assert(!"Invalid type.");
		return nullptr;
	}
}

Decl* Decl::CastFromDeclContext(const DeclContext* declContext)
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

nBool Decl::IsFunction() const noexcept
{
	return m_Type >= FirstFunction && m_Type <= LastFunction;
}

void Decl::SetNextDeclInContext(natRefPointer<Decl> value) noexcept
{
	m_NextDeclInContext = value;
}

const char* DeclContext::GetTypeName() const noexcept
{
	return getTypeName(m_Type);
}

Linq<DeclPtr> DeclContext::GetDecls() const
{
	return from(DeclIterator{ m_FirstDecl }, DeclIterator{});
}

void DeclContext::AddDecl(DeclPtr decl)
{
	if (m_FirstDecl)
	{
		m_LastDecl->SetNextDeclInContext(decl);
		m_LastDecl = decl;
	}
	else
	{
		m_FirstDecl = m_LastDecl = decl;
	}

	OnNewDeclAdded(std::move(decl));
}

void DeclContext::RemoveDecl(DeclPtr const& decl)
{
	if (decl == m_FirstDecl)
	{
		if (decl == m_LastDecl)
		{
			m_FirstDecl = m_LastDecl = 0;
		}
		else
		{
			m_FirstDecl = decl->GetNextDeclInContext();
		}
	}
	else
	{
		for (auto d = m_FirstDecl; d; d = d->GetNextDeclInContext())
		{
			if (d->GetNextDeclInContext() == decl)
			{
				d->SetNextDeclInContext(decl);
				if (decl == m_LastDecl)
				{
					m_LastDecl = d;
				}
			}
		}
	}

	decl->SetNextDeclInContext(nullptr);
}

nBool DeclContext::ContainsDecl(DeclPtr const& decl)
{
	return decl->GetContext() == this && (decl->GetNextDeclInContext() || decl == m_LastDecl);
}

Linq<natRefPointer<NamedDecl>> DeclContext::Lookup(natRefPointer<Identifier::IdentifierInfo> const& info) const
{
	return GetDecls().where([&info] (DeclPtr const& decl)
	{
		const auto namedDecl = static_cast<natRefPointer<NamedDecl>>(decl);
		if (!namedDecl)
		{
			return false;
		}
		return namedDecl->GetIdentifierInfo() == info;
	}).select([] (DeclPtr const& decl) { return static_cast<natRefPointer<NamedDecl>>(decl); });
}

DeclContext::DeclIterator::DeclIterator(DeclPtr firstDecl)
	: m_Current{ std::move(firstDecl) }
{
}

DeclPtr DeclContext::DeclIterator::operator*() const noexcept
{
	return m_Current;
}

DeclContext::DeclIterator& DeclContext::DeclIterator::operator++() & noexcept
{
	if (m_Current)
	{
		m_Current = m_Current->GetNextDeclInContext();
	}

	return *this;
}

nBool DeclContext::DeclIterator::operator==(DeclIterator const& other) const noexcept
{
	return m_Current == other.m_Current;
}

nBool DeclContext::DeclIterator::operator!=(DeclIterator const& other) const noexcept
{
	return m_Current != other.m_Current;
}

void DeclContext::OnNewDeclAdded(DeclPtr /*decl*/)
{
}
