#pragma once
#include <natLinq.h>

#include "CompilerAction.h"
#include <optional>
#include "AST/ASTContext.h"

namespace NatsuLang
{
	namespace Diag
	{
		class DiagnosticsEngine;
	}

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

		nStrView GetName() const noexcept override;

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

		nStrView GetName() const noexcept override;

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

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void StartAction(CompilerActionContext const& context) override;
		void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

	private:
		std::optional<nBool> m_Result;
		Semantic::Sema* m_Sema;
		static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
	};

	// 此 Action 不允许用户自己调用，也不会进行注册，它将作为 Arg 系 Action 的返回值传递给 ActionTemplate 等，仅用于传递数据
	class ActionArgInfo
		: public NatsuLib::natRefObjImpl<ActionArgInfo, ICompilerAction>
	{
	public:
		enum class ArgType
		{
			Type,
			Expr,
		};

		// TODO: 需要 argType 吗？
		ActionArgInfo(ArgType argType, NatsuLib::natRefPointer<ASTNode> arg);
		~ActionArgInfo();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void StartAction(CompilerActionContext const& context) override;
		void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

		ArgType GetArgType() const noexcept
		{
			return m_ArgType;
		}

		NatsuLib::natRefPointer<ASTNode> GetArg() const noexcept
		{
			return m_Arg;
		}

	private:
		ArgType m_ArgType;
		NatsuLib::natRefPointer<ASTNode> m_Arg;
	};

	class ActionTypeArg
		: public NatsuLib::natRefObjImpl<ActionTypeArg, ICompilerAction>
	{
	public:
		ActionTypeArg();
		~ActionTypeArg();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void StartAction(CompilerActionContext const& context) override;
		void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

	private:
		static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
		Diag::DiagnosticsEngine* m_Diag;
		Identifier::IdPtr m_TypeId;
	};

	class ActionTemplate
		: public NatsuLib::natRefObjImpl<ActionTemplate, ICompilerAction>
	{
	public:
		ActionTemplate();
		~ActionTemplate();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void StartAction(CompilerActionContext const& context) override;
		void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

		void EndArgumentList() override;

	private:
		class ActionTemplateArgumentRequirement
			: public natRefObjImpl<ActionTemplateArgumentRequirement, IArgumentRequirement>
		{
		public:
			explicit ActionTemplateArgumentRequirement(ActionTemplate& action)
				: m_Action{ action }
			{
			}

			~ActionTemplateArgumentRequirement();

			CompilerActionArgumentType GetExpectedArgumentType(std::size_t i) override;

		private:
			ActionTemplate& m_Action;
		};

		Semantic::Sema* m_Sema;
		nBool m_IsTemplateArgEnded;
	};
}
