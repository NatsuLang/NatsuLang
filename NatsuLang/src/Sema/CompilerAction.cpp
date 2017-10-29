#include "Sema/CompilerAction.h"

using namespace NatsuLib;
using namespace NatsuLang;

IArgumentRequirement::~IArgumentRequirement()
{
}

ICompilerAction::~ICompilerAction()
{
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
	return m_SubNamespace.emplace(subNamespace->GetName(), subNamespace).second;
}

nBool CompilerActionNamespace::RegisterAction(natRefPointer<ICompilerAction> const& action)
{
	return m_Actions.emplace(action->GetName(), action).second;
}
