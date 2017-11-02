#pragma once
#include <natLinq.h>

#include "CompilerAction.h"
#include <optional>
#include "AST/ASTContext.h"

namespace NatsuLang
{
	namespace Semantic
	{
		class Sema;
	}

	class SimpleArgumentRequirement
		: public NatsuLib::natRefObjImpl<SimpleArgumentRequirement, IArgumentRequirement>
	{
	public:
		explicit SimpleArgumentRequirement(std::initializer_list<CompilerActionArgumentType> const& types);
		explicit SimpleArgumentRequirement(NatsuLib::Linq<NatsuLib::Valued<CompilerActionArgumentType>> const& types);
		~SimpleArgumentRequirement();

		CompilerActionArgumentType GetExpectedArgumentType(std::size_t i) override;

	private:
		std::vector<CompilerActionArgumentType> m_Types;
	};

	class ActionDump
		: public NatsuLib::natRefObjImpl<ActionDump, ICompilerAction>
	{
	public:
		ActionDump();
		~ActionDump();

		nString GetName() const noexcept override;

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void StartAction(CompilerActionContext const& context) override;
		void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

	private:
		struct ActionDumpArgumentRequirement
			: natRefObjImpl<ActionDumpArgumentRequirement, IArgumentRequirement>
		{
			~ActionDumpArgumentRequirement();

			CompilerActionArgumentType GetExpectedArgumentType(std::size_t i) override;
		};

		static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
		std::vector<NatsuLib::natRefPointer<ASTNode>> m_ResultNodes;
	};

	class ActionDumpIf
		: public NatsuLib::natRefObjImpl<ActionDumpIf, ICompilerAction>
	{
	public:
		ActionDumpIf();
		~ActionDumpIf();

		nString GetName() const noexcept override;

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void StartAction(CompilerActionContext const& context) override;
		void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

	private:
		NatsuLib::natRefPointer<ASTContext> m_Context;
		std::optional<nBool> m_SkipThisNode;
		NatsuLib::natRefPointer<ASTNode> m_ResultNode;
		static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
	};

	class ActionIsDefined
		: public NatsuLib::natRefObjImpl<ActionIsDefined, ICompilerAction>
	{
	public:
		ActionIsDefined();
		~ActionIsDefined();

		nString GetName() const noexcept override;

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void StartAction(CompilerActionContext const& context) override;
		void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

	private:
		std::optional<nBool> m_Result;
		Semantic::Sema* m_Sema;
		static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
	};
}
