#pragma once
#include <AST/Expression.h>
#include <AST/ASTConsumer.h>

#include <llvm/IR/Module.h>

namespace NatsuLang::Compiler
{
	class AotAstConsumer
		: public NatsuLib::natRefObjImpl<AotAstConsumer, ASTConsumer>
	{
	public:
		AotAstConsumer();
		~AotAstConsumer();

		void Initialize(ASTContext& context) override;
		void HandleTranslationUnit(ASTContext& context) override;
		nBool HandleTopLevelDecl(NatsuLib::Linq<NatsuLib::Valued<Declaration::DeclPtr>> const& decls) override;

	private:
		std::unique_ptr<llvm::Module> m_Module;
	};
}
