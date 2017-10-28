#pragma once
#include <natRefObj.h>

namespace NatsuLang
{
	struct ASTNode
		: NatsuLib::natRefObj
	{
		virtual ~ASTNode();
	};
}
