#include "AST/DeclBase.h"
#include "AST/Declaration.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;
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

IAttribute::~IAttribute()
{
}

Decl::~Decl()
{
}

DeclContext* Decl::CastToDeclContext(const Decl* decl)
{
	if (!decl)
	{
		return nullptr;
	}

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
		return nullptr;
	}
}

Decl* Decl::CastFromDeclContext(const DeclContext* declContext)
{
	if (!declContext)
	{
		return nullptr;
	}

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

void Decl::AttachAttribute(AttrPtr attr)
{
	const auto& ref = *attr;
	m_AttributeSet[typeid(ref)].emplace(std::move(attr));
}

nBool Decl::DetachAttribute(AttrPtr const& attr)
{
	const auto& ref = *attr;
	if (const auto iter = m_AttributeSet.find(typeid(ref)); iter != m_AttributeSet.cend())
	{
		return iter->second.erase(attr);
	}

	return false;
}

std::size_t Decl::GetAttributeTotalCount() const noexcept
{
	std::size_t size{};
	for (const auto& set : m_AttributeSet)
	{
		size += set.second.size();
	}
	return size;
}

std::size_t Decl::GetAttributeCount(std::type_index const& type) const noexcept
{
	if (const auto iter = m_AttributeSet.find(type); iter != m_AttributeSet.cend())
	{
		return iter->second.size();
	}

	return 0;
}

std::size_t Decl::GetAttributeTypeCount() const noexcept
{
	return m_AttributeSet.size();
}

Linq<Valued<AttrPtr>> Decl::GetAllAttributes() const noexcept
{
	Linq<Valued<AttrPtr>> query = from_empty<AttrPtr>();
	for (const auto& set : m_AttributeSet)
	{
		query = query.concat(from(set.second));
	}
	return query;
}

Linq<Valued<AttrPtr>> Decl::GetAttributes(std::type_index const& type) const noexcept
{
	const auto iter = m_AttributeSet.find(type);
	if (iter == m_AttributeSet.cend())
	{
		return from_empty<AttrPtr>();
	}

	return from(iter->second);
}

void Decl::SetNextDeclInContext(natRefPointer<Decl> value) noexcept
{
	m_NextDeclInContext = value;
}

const char* DeclContext::GetTypeName() const noexcept
{
	return getTypeName(m_Type);
}

Linq<Valued<DeclPtr>> DeclContext::GetDecls() const
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
			m_FirstDecl = m_LastDecl = nullptr;
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
				d->SetNextDeclInContext(decl->GetNextDeclInContext());
				if (decl == m_LastDecl)
				{
					m_LastDecl = d;
				}
			}
		}
	}

	decl->SetNextDeclInContext(nullptr);
	decl->SetContext(nullptr);
}

void DeclContext::RemoveAllDecl()
{
	for (auto decl = m_FirstDecl; decl;)
	{
		auto next = decl->GetNextDeclInContext();
		decl->SetNextDeclInContext(nullptr);
		decl->SetContext(nullptr);
		decl = std::move(next);
	}

	m_FirstDecl = m_LastDecl = nullptr;
}

nBool DeclContext::ContainsDecl(DeclPtr const& decl)
{
	return decl->GetContext() == this && (decl->GetNextDeclInContext() || decl == m_LastDecl);
}

NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<NamedDecl>>> DeclContext::Lookup(
	natRefPointer<Identifier::IdentifierInfo> const& info) const
{
	return GetDecls().where([info] (DeclPtr const& decl)
	{
		const auto namedDecl = decl.Cast<NamedDecl>();
		if (!namedDecl)
		{
			return false;
		}
		return namedDecl->GetIdentifierInfo() == info;
	}).select([] (DeclPtr const& decl) { return decl.Cast<NamedDecl>(); });
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
