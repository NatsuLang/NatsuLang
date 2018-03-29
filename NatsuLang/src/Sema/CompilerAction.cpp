#include "Sema/CompilerAction.h"

using namespace NatsuLib;
using namespace NatsuLang;

IArgumentRequirement::~IArgumentRequirement()
{
}

ICompilerAction::~ICompilerAction()
{
}

void ICompilerAction::EndArgumentList()
{
}

void ICompilerAction::EndArgumentSequence()
{
}

void ICompilerAction::AttachTo(natWeakRefPointer<CompilerActionNamespace> parent) noexcept
{
	m_Parent = parent;
}

natWeakRefPointer<CompilerActionNamespace> ICompilerAction::GetParent() const noexcept
{
	return m_Parent;
}

CompilerActionNamespace::CompilerActionNamespace(nString name)
	: m_Name{ std::move(name) }
{
}

CompilerActionNamespace::~CompilerActionNamespace()
{
}

nStrView CompilerActionNamespace::GetName() const noexcept
{
	return m_Name;
}

natRefPointer<CompilerActionNamespace> CompilerActionNamespace::GetSubNamespace(nStrView name)
{
	const auto iter = m_SubNamespace.find(name);
	if (iter != m_SubNamespace.end())
	{
		return iter->second;
	}

	return nullptr;
}

natRefPointer<ICompilerAction> CompilerActionNamespace::GetAction(nStrView name)
{
	const auto iter = m_Actions.find(name);
	if (iter != m_Actions.end())
	{
		return iter->second;
	}

	return nullptr;
}

nBool CompilerActionNamespace::RegisterSubNamespace(nStrView name)
{
	const auto subNamespace = make_ref<CompilerActionNamespace>(name);
	if (m_SubNamespace.emplace(subNamespace->GetName(), subNamespace).second)
	{
		subNamespace->m_Parent = ForkWeakRef();
		return true;
	}

	return false;
}

nBool CompilerActionNamespace::RegisterAction(natRefPointer<ICompilerAction> const& action)
{
	if (m_Actions.emplace(action->GetName(), action).second)
	{
		action->AttachTo(ForkWeakRef());
		return true;
	}

	return false;
}

natWeakRefPointer<CompilerActionNamespace> CompilerActionNamespace::GetParent() const noexcept
{
	return m_Parent;
}
