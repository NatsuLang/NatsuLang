#pragma once
#include <natMisc.h>

#include "AST/ASTNode.h"

namespace NatsuLang
{
	namespace Syntax
	{
		class Parser;
	}

	enum class CompilerActionArgumentType
	{
		None		= 0x0,	///< @brief	无参数

		Optional	= 0x1,	///< @brief	可选参数，不能单独设置

		Type		= 0x2,	///< @brief	类型参数
		Declaration	= 0x4,	///< @brief	声明参数
		Statement	= 0x8,	///< @brief	语句参数
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(CompilerActionArgumentType);

	struct IArgumentRequirement
		: NatsuLib::natRefObj
	{
		virtual ~IArgumentRequirement();

		///	@brief	返回期望的指定参数类型
		///	@param	i	指定的从 0 开始的第 i 个参数
		///	@return	期望的参数类型，None 表示要求不接受第 i 个以上的参数，迭代可以以此结束
		virtual CompilerActionArgumentType GetExpectedArgumentType(std::size_t i) = 0;
	};

	class CompilerActionContext final
	{
	public:
		constexpr explicit CompilerActionContext(Syntax::Parser& parser) noexcept
			: m_Parser{ &parser }
		{
		}

		constexpr Syntax::Parser& GetParser() const noexcept
		{
			return *m_Parser;
		}

	private:
		Syntax::Parser* m_Parser;
	};

	struct ICompilerAction
		: NatsuLib::natRefObj
	{
		virtual ~ICompilerAction();

		///	@brief	获取此动作的名称
		///	@remark	必须保证返回值在整个生存期内有效且不改变
		virtual nStrView GetName() const noexcept = 0;

		///	@brief	获取参数要求
		///	@remark	必须在 StartAction 方法执行后再执行
		///	@return	参数要求对象，仅保证在 EndAction 之前有效
		virtual NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() = 0;

		///	@brief	以指定的上下文开始此动作的执行
		///	@param	context	动作的上下文
		virtual void StartAction(CompilerActionContext const& context) = 0;

		///	@brief	结束本次动作的执行，并清理自上次 StartAction 以来的所有状态
		///	@param	output	获取输出的抽象语法树节点的回调函数，若返回 true 则立即中止获取，本函数将立刻返回
		virtual void EndAction(std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output = {}) = 0;

		///	@brief	添加参数
		///	@param	arg		本次添加的参数
		virtual void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) = 0;
	};

	class CompilerActionNamespace final
		: public NatsuLib::natRefObjImpl<CompilerActionNamespace>
	{
	public:
		explicit CompilerActionNamespace(nString name);
		~CompilerActionNamespace();

		nStrView GetName() const noexcept;

		NatsuLib::natRefPointer<CompilerActionNamespace> GetSubNamespace(nStrView name);
		NatsuLib::natRefPointer<ICompilerAction> GetAction(nStrView name);

		nBool RegisterSubNamespace(nStrView name);
		nBool RegisterAction(NatsuLib::natRefPointer<ICompilerAction> const& action);

	private:
		const nString m_Name;
		std::unordered_map<nStrView, NatsuLib::natRefPointer<CompilerActionNamespace>> m_SubNamespace;
		std::unordered_map<nStrView, NatsuLib::natRefPointer<ICompilerAction>> m_Actions;
	};
}
