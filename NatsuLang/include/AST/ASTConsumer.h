#pragma once
#include "ASTContext.h"

namespace NatsuLang
{
	struct ASTConsumer
		: NatsuLib::natRefObj
	{
		~ASTConsumer();

		virtual void Initialize(ASTContext& context) = 0;
		virtual void HandleTranslationUnit(ASTContext& context) = 0;
		
		///	@brief	处理顶层声明
		///	@param	decls	已分析的声明
		///	@return	返回 false 表示需要中止分析，ParseAST 将会立刻返回
		virtual nBool HandleTopLevelDecl(NatsuLib::Linq<const Declaration::DeclPtr> const& decls) = 0;
	};
}
