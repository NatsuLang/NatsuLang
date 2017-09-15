#include "Sema/Scope.h"

using namespace NatsuLang::Semantic;

void Scope::SetFlags(NatsuLib::natRefPointer<Scope> parent, ScopeFlags flags) noexcept
{
	m_Parent = parent;
	m_Flags = flags;
	m_Entity = nullptr;

	if (parent && (flags & ScopeFlags::FunctionScope) == ScopeFlags::None)
	{
		m_BreakParent = parent->m_BreakParent;
		m_ContinueParent = parent->m_ContinueParent;
	}
	else
	{
		m_BreakParent.Reset();
		m_ContinueParent.Reset();
	}

	if (parent)
	{
		m_Depth = parent->m_Depth + 1;
		m_BlockParent = parent->m_BlockParent;
		m_FunctionParent = parent->m_FunctionParent;
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
