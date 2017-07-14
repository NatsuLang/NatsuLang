#include "Sema/Scope.h"

using namespace NatsuLang::Semantic;

void Scope::SetFlags(NatsuLib::natWeakRefPointer<Scope> parent, ScopeFlags flags) noexcept
{
	const auto strongRefParent = parent.Lock();

	m_Parent = parent;
	m_Flags = flags;

	if (strongRefParent && (flags & ScopeFlags::FunctionScope) == ScopeFlags::None)
	{
		m_BreakParent = strongRefParent->m_BreakParent;
		m_ContinueParent = strongRefParent->m_ContinueParent;
	}
	else
	{
		m_BreakParent.Reset();
		m_ContinueParent.Reset();
	}

	if (strongRefParent)
	{
		m_Depth = strongRefParent->m_Depth + 1;
		m_BlockParent = strongRefParent->m_BlockParent;
		m_FunctionParent = strongRefParent->m_FunctionParent;
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
