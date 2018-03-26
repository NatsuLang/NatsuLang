#pragma once
#include <natRefObj.h>

namespace NatsuLang
{
	struct ASTNode
		: NatsuLib::natRefObj
	{
		virtual ~ASTNode();
	};

	using ASTNodePtr = NatsuLib::natRefPointer<ASTNode>;
}
