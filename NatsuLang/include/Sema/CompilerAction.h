#pragma once
#include <natMisc.h>
#include <natString.h>

#include "AST/ASTNode.h"

namespace NatsuLang
{
	namespace Syntax
	{
		class Parser;
	}

	///	@brief	编译器动作参数类型
	enum class CompilerActionArgumentType
	{
		None			= 0x0000,	///< @brief	无参数

		Optional		= 0x0001,	///< @brief	可选参数，不能单独设置
		MayBeUnresolved	= 0x0002,	///< @brief	声明可为 UnresolvedDecl，仅能和 Declaration 同时设置

		MayBeSingle		= 0x0004,	///< @brief 该参数可为单独参数（即括号结束后的参数）
		MayBeSeq		= 0x0008,	///< @brief 该参数可为序列参数（即花括号中的参数）

		Type			= 0x0100,	///< @brief	类型参数
		Declaration		= 0x0200,	///< @brief	声明参数
		Statement		= 0x0400,	///< @brief	语句参数
		Identifier		= 0x0800,	///< @brief	标识符，将作为 UnresolvedDecl 传入，不会进行名称查找，GetDeclaratorPtr 将会返回 nullptr
		CompilerAction	= 0x1000,	///< @brief	编译器动作
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(CompilerActionArgumentType);

	constexpr CompilerActionArgumentType GetCategoryPart(CompilerActionArgumentType value) noexcept
	{
		using UnderlyingType = std::underlying_type_t<CompilerActionArgumentType>;
		return static_cast<CompilerActionArgumentType>(static_cast<UnderlyingType>(value) & UnderlyingType{ 0xFF00 });
	}

	struct IArgumentRequirement
		: NatsuLib::natRefObj
	{
		virtual ~IArgumentRequirement();

		///	@brief	返回下一个期望的参数类型
		///	@return	期望的参数类型，None 表示要求不接受更多参数，迭代可以以此结束
		///	@remark	此方法可能根据已输入的参数而改变返回值，请根据实际编译器动作的说明来了解此方法可能的行为
		virtual CompilerActionArgumentType GetNextExpectedArgumentType() = 0;
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

	class CompilerActionNamespace;

	struct IActionContext
		: NatsuLib::natRefObj
	{
		virtual ~IActionContext();

		///	@brief	获取参数要求
		///	@return	参数要求对象，仅保证在 EndAction 之前有效
		virtual NatsuLib::natRefPointer<IArgumentRequirement> GetArgumentRequirement() = 0;

		///	@brief	添加参数
		///	@param	arg		本次添加的参数
		virtual void AddArgument(NatsuLib::natRefPointer<ASTNode> const& arg) = 0;

		///	@brief	结束参数列表
		virtual void EndArgumentList();

		///	@brief	结束参数序列
		virtual void EndArgumentSequence();
	};

	///	@brief	编译器动作接口
	struct ICompilerAction
		: ASTNode
	{
		virtual ~ICompilerAction();

		///	@brief	获取此动作的名称
		///	@remark	必须保证返回值在整个生存期内有效且不改变
		virtual nStrView GetName() const noexcept = 0;

		///	@brief	以指定的上下文开始此动作的执行
		///	@param	context	动作的上下文
		///	@return	本次执行的上下文
		virtual NatsuLib::natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) = 0;

		///	@brief	结束本次动作的执行，并使 context 无效，此 context 之后不能再次被使用
		///	@param	context 本次执行的上下文
		///	@param	output	获取输出的抽象语法树节点的回调函数，若返回 true 则立即中止获取，本函数将立刻返回
		virtual void EndAction(NatsuLib::natRefPointer<IActionContext> const& context, std::function<nBool(NatsuLib::natRefPointer<ASTNode>)> const& output = {}) = 0;

		void AttachTo(NatsuLib::natWeakRefPointer<CompilerActionNamespace> parent) noexcept;
		NatsuLib::natWeakRefPointer<CompilerActionNamespace> GetParent() const noexcept;

	private:
		NatsuLib::natWeakRefPointer<CompilerActionNamespace> m_Parent;
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

		NatsuLib::natWeakRefPointer<CompilerActionNamespace> GetParent() const noexcept;

	private:
		const nString m_Name;
		NatsuLib::natWeakRefPointer<CompilerActionNamespace> m_Parent;
		std::unordered_map<nStrView, NatsuLib::natRefPointer<CompilerActionNamespace>> m_SubNamespace;
		std::unordered_map<nStrView, NatsuLib::natRefPointer<ICompilerAction>> m_Actions;
	};
}
