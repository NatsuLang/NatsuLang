#include "Sema/DefaultActions.h"
#include "AST/Expression.h"
#include "Parse/Parser.h"
#include "Sema/Sema.h"
#include "Sema/Scope.h"

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

const natRefPointer<IArgumentRequirement> ActionDumpIf::s_ArgumentRequirement
{
	make_ref<SimpleArgumentRequirement>(
	std::initializer_list<CompilerActionArgumentType>{
			CompilerActionArgumentType::Statement,
			CompilerActionArgumentType::Type |
			CompilerActionArgumentType::Declaration |
			CompilerActionArgumentType::Statement,
			CompilerActionArgumentType::Optional |
			CompilerActionArgumentType::Type |
			CompilerActionArgumentType::Declaration |
			CompilerActionArgumentType::Statement
		}
	)
};

ActionDumpIf::ActionDumpIf()
{
}

ActionDumpIf::~ActionDumpIf()
{
}

nString ActionDumpIf::GetName() const noexcept
{
	return "DumpIf";
}

natRefPointer<IArgumentRequirement> ActionDumpIf::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionDumpIf::StartAction(CompilerActionContext const& context)
{
	m_Context = context.GetParser().GetSema().GetASTContext().ForkRef();
}

void ActionDumpIf::EndAction(std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	if (output)
	{
		output(m_ResultNode);
	}

	m_Context.Reset();
	m_SkipThisNode.reset();
	m_ResultNode.Reset();
}

void ActionDumpIf::AddArgument(natRefPointer<ASTNode> const& arg)
{
	if (!m_SkipThisNode)
	{
		const auto conditionExpr = static_cast<Expression::ExprPtr>(arg);
		if (!conditionExpr)
		{
			// TODO: 报告错误
			return;
		}

		nuLong result;
		if (!conditionExpr->EvaluateAsInt(result, *m_Context))
		{
			// TODO: 报告错误
			return;
		}

		m_SkipThisNode = !result;
	}
	else
	{
		if (!m_SkipThisNode.value())
		{
			m_ResultNode = arg;
		}

		m_SkipThisNode = !m_SkipThisNode.value();
	}
}

const natRefPointer<IArgumentRequirement> ActionIsDefined::s_ArgumentRequirement{ make_ref<SimpleArgumentRequirement>(CompilerActionArgumentType::Statement) };

ActionIsDefined::ActionIsDefined()
	: m_Sema{ nullptr }
{
}

ActionIsDefined::~ActionIsDefined()
{
}

nString ActionIsDefined::GetName() const noexcept
{
	return "IsDefined";
}

natRefPointer<IArgumentRequirement> ActionIsDefined::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionIsDefined::StartAction(CompilerActionContext const& context)
{
	m_Sema = &context.GetParser().GetSema();
}

void ActionIsDefined::EndAction(std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	if (output)
	{
		output(m_Result ? make_ref<Expression::BooleanLiteral>(m_Result.value(),
			m_Sema->GetASTContext().GetBuiltinType(Type::BuiltinType::Bool), SourceLocation{}) : nullptr);
	}

	m_Result.reset();
	m_Sema = nullptr;
}

void ActionIsDefined::AddArgument(natRefPointer<ASTNode> const& arg)
{
	const auto name = static_cast<natRefPointer<Expression::StringLiteral>>(arg);
	if (!name)
	{
		// TODO: 报告错误
		return;
	}

	Lex::Token dummyToken;
	Semantic::LookupResult r{ *m_Sema,
		m_Sema->GetPreprocessor().FindIdentifierInfo(name->GetValue(), dummyToken),
		SourceLocation{}, Semantic::Sema::LookupNameType::LookupAnyName };
	m_Result = m_Sema->LookupName(r, m_Sema->GetCurrentScope()) && r.GetDeclSize();
}
