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

	class SimpleActionContext
		: public NatsuLib::natRefObjImpl<SimpleActionContext, IActionContext>
	{
	public:
		explicit SimpleActionContext(NatsuLib::natRefPointer<IArgumentRequirement> requirement);
		~SimpleActionContext();

		NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
		void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

		std::vector<ASTNodePtr> const& GetArguments() const noexcept;

	private:
		NatsuLib::natRefPointer<IArgumentRequirement> m_Requirement;
		std::vector<ASTNodePtr> m_ArgumentList;
	};

	class ActionDump
		: public NatsuLib::natRefObjImpl<ActionDump, ICompilerAction>
	{
	public:
		ActionDump();
		~ActionDump();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		struct ActionDumpArgumentRequirement
			: natRefObjImpl<ActionDumpArgumentRequirement, IArgumentRequirement>
		{
			~ActionDumpArgumentRequirement();

			CompilerActionArgumentType GetExpectedArgumentType(std::size_t i) override;
		};

		static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
	};

	class ActionDumpIf
		: public NatsuLib::natRefObjImpl<ActionDumpIf, ICompilerAction>
	{
	public:
		ActionDumpIf();
		~ActionDumpIf();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		struct ActionDumpIfContext
			: natRefObjImpl<ActionDumpIfContext, IActionContext>
		{
			~ActionDumpIfContext();

			NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
			void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

			static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;

			NatsuLib::natRefPointer<ASTContext> Context;
			std::optional<nBool> SkipThisNode;
			NatsuLib::natRefPointer<ASTNode> ResultNode;
		};
	};

	class ActionIsDefined
		: public NatsuLib::natRefObjImpl<ActionIsDefined, ICompilerAction>
	{
	public:
		ActionIsDefined();
		~ActionIsDefined();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		struct ActionIsDefinedContext
			: natRefObjImpl<ActionIsDefinedContext, IActionContext>
		{
			~ActionIsDefinedContext();

			NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
			void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

			static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
			std::optional<nBool> Result;
			Semantic::Sema* Sema;
		};
	};

	class ActionTypeOf
		: public NatsuLib::natRefObjImpl<ActionTypeOf, ICompilerAction>
	{
	public:
		ActionTypeOf();
		~ActionTypeOf();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		struct ActionTypeOfContext
			: natRefObjImpl<ActionTypeOfContext, IActionContext>
		{
			~ActionTypeOfContext();

			NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
			void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

			static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
			Type::TypePtr Type;
		};
	};

	class ActionSizeOf
		: public NatsuLib::natRefObjImpl<ActionSizeOf, ICompilerAction>
	{
	public:
		ActionSizeOf();
		~ActionSizeOf();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		struct ActionSizeOfContext
			: natRefObjImpl<ActionSizeOfContext, IActionContext>
		{
			explicit ActionSizeOfContext(ASTContext& astContext);
			~ActionSizeOfContext();

			NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
			void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

			static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
			ASTContext& ASTContext;
			std::optional<nuLong> Value;
		};
	};

	class ActionAlignOf
		: public NatsuLib::natRefObjImpl<ActionAlignOf, ICompilerAction>
	{
	public:
		ActionAlignOf();
		~ActionAlignOf();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		struct ActionAlignOfContext
			: natRefObjImpl<ActionAlignOfContext, IActionContext>
		{
			explicit ActionAlignOfContext(ASTContext& astContext);
			~ActionAlignOfContext();

			NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
			void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

			static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
			ASTContext& ASTContext;
			std::optional<nuLong> Value;
		};
	};

	// $Compiler.CreateAt(ptr [, initializer-list])
	class ActionCreateAt
		: public NatsuLib::natRefObjImpl<ActionCreateAt, ICompilerAction>
	{
	public:
		ActionCreateAt();
		~ActionCreateAt();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		class ActionCreateAtArgumentRequirement
			: public natRefObjImpl<ActionCreateAtArgumentRequirement, IArgumentRequirement>
		{
		public:
			ActionCreateAtArgumentRequirement();
			~ActionCreateAtArgumentRequirement();

			CompilerActionArgumentType GetExpectedArgumentType(std::size_t i) override;
		};

		struct ActionCreateAtContext
			: natRefObjImpl<ActionCreateAtContext, IActionContext>
		{
			~ActionCreateAtContext();

			NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
			void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

			static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
			Semantic::Sema* Sema;
			Expression::ExprPtr Ptr;
			std::vector<Expression::ExprPtr> Arguments;
		};
	};

	// $Compiler.DestroyAt(ptr);
	class ActionDestroyAt
		: public NatsuLib::natRefObjImpl<ActionDestroyAt, ICompilerAction>
	{
	public:
		ActionDestroyAt();
		~ActionDestroyAt();

		nStrView GetName() const noexcept override;

		NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override;
		void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output) override;

	private:
		struct ActionDestroyAtContext
			: natRefObjImpl<ActionDestroyAtContext, IActionContext>
		{
			~ActionDestroyAtContext();

			NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() override;
			void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) override;

			static const NatsuLib::natRefPointer<IArgumentRequirement> s_ArgumentRequirement;
			Semantic::Sema* Sema;
			Expression::ExprPtr Ptr;
		};
	};
}
