#pragma once
#include <AST/Expression.h>
#include <AST/ASTConsumer.h>

namespace NatsuLang::Interpreter
{
	class InterpreterASTConsumer
		: public NatsuLib::natRefObjImpl<InterpreterASTConsumer, ASTConsumer>
	{
	public:
		InterpreterASTConsumer();
		~InterpreterASTConsumer();

		void Initialize(ASTContext& context) override;
		void HandleTranslationUnit(ASTContext& context) override;
		nBool HandleTopLevelDecl(NatsuLib::Linq<NatsuLib::Valued<Declaration::DeclPtr>> const& decls) override;

	private:

	};
}
