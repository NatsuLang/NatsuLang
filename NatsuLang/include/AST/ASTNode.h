#pragma once
#include <natRefObj.h>

namespace NatsuLang
{
	// 可能要访问私有成员，因此不使用 Visitor
	// 考虑通过接口来做
	struct ASTNode
		: NatsuLib::natRefObj
	{
		virtual ~ASTNode();
	};

	using ASTNodePtr = NatsuLib::natRefPointer<ASTNode>;

	enum class ASTNodeType : nByte
	{
		Declaration,
		Statement,
		Type,
		CompilerAction,

		ASTNodeTypeCount
	};
}
