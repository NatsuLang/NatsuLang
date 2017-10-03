#include "Sema/Scope.h"

using namespace NatsuLang::Semantic;

void Scope::SetFlags(NatsuLib::natRefPointer<Scope> parent, ScopeFlags flags) noexcept
{
	m_Parent = std::move(parent);
	m_Flags = flags;
	m_Entity = nullptr;

	if (m_Parent && (flags & ScopeFlags::FunctionScope) == ScopeFlags::None)
	{
		m_BreakParent = m_Parent->m_BreakParent;
		m_ContinueParent = m_Parent->m_ContinueParent;
	}
	else
	{
		m_BreakParent.Reset();
		m_ContinueParent.Reset();
	}

	if (m_Parent)
	{
		m_Depth = m_Parent->m_Depth + 1;
		m_BlockParent = m_Parent->m_BlockParent;
		m_FunctionParent = m_Parent->m_FunctionParent;
	}
	else
	{
		m_Depth = 0;
		m_BlockParent.Reset();
		m_FunctionParent.Reset();
	}

	if ((m_Flags & ScopeFlags::FunctionScope) != ScopeFlags::None)
	{
		m_FunctionParent = ForkWeakRef();
	}

	if ((m_Flags & ScopeFlags::BreakableScope) != ScopeFlags::None)
	{
		m_BreakParent = ForkWeakRef();
	}

	if ((m_Flags & ScopeFlags::ContinuableScope) != ScopeFlags::None)
	{
		m_ContinueParent = ForkWeakRef();
	}
}

void Scope::AddFlags(ScopeFlags flags) noexcept
{
	if ((flags & ScopeFlags::BreakableScope) != ScopeFlags::None)
	{
		assert((m_Flags & ScopeFlags::BreakableScope) == ScopeFlags::None);
		m_BreakParent = ForkWeakRef();
	}

	if ((flags & ScopeFlags::ContinuableScope) != ScopeFlags::None)
	{
		assert((m_Flags & ScopeFlags::ContinuableScope) == ScopeFlags::None);
		m_ContinueParent = ForkWeakRef();
	}

	m_Flags |= flags;
}
