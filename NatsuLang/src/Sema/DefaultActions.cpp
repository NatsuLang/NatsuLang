#include "Sema/DefaultActions.h"

using namespace NatsuLib;
using namespace NatsuLang;

SimpleArgumentRequirement::SimpleArgumentRequirement(std::initializer_list<CompilerActionArgumentType> const& types)
	: m_Types(types.begin(), types.end())
{
}

SimpleArgumentRequirement::SimpleArgumentRequirement(Linq<Valued<CompilerActionArgumentType>> const& types)
	: m_Types(types.begin(), types.end())
{
}

SimpleArgumentRequirement::~SimpleArgumentRequirement()
{
}

CompilerActionArgumentType SimpleArgumentRequirement::GetExpectedArgumentType(std::size_t i)
{
	return i < m_Types.size() ? m_Types[i] : CompilerActionArgumentType::None;
}

const natRefPointer<IArgumentRequirement> ActionDump::s_ArgumentRequirement{ make_ref<ActionDumpArgumentRequirement>() };

ActionDump::ActionDump()
{
}

ActionDump::~ActionDump()
{
}

nString ActionDump::GetName() const noexcept
{
	return "Dump";
}

natRefPointer<IArgumentRequirement> ActionDump::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionDump::StartAction(CompilerActionContext const& /*context*/)
{
}

void ActionDump::EndAction(std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	if (output)
	{
		const auto scope = make_scope([this]
		{
			m_ResultNodes.clear();
		});

		for (auto&& node : m_ResultNodes)
		{
			if (output(std::move(node)))
			{
				return;
			}
		}
	}
}

void ActionDump::AddArgument(natRefPointer<ASTNode> const& arg)
{
	m_ResultNodes.emplace_back(arg);
}

ActionDump::ActionDumpArgumentRequirement::~ActionDumpArgumentRequirement()
{
}

CompilerActionArgumentType ActionDump::ActionDumpArgumentRequirement::GetExpectedArgumentType(std::size_t /*i*/)
{
	return CompilerActionArgumentType::Optional |
		CompilerActionArgumentType::Type |
		CompilerActionArgumentType::Declaration |
		CompilerActionArgumentType::Statement;
}
