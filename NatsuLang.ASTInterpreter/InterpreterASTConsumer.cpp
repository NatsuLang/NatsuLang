#include "InterpreterASTConsumer.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Interpreter;

InterpreterASTConsumer::InterpreterASTConsumer()
{
}

InterpreterASTConsumer::~InterpreterASTConsumer()
{
}

void InterpreterASTConsumer::Initialize(ASTContext& context)
{
}

void InterpreterASTConsumer::HandleTranslationUnit(ASTContext& context)
{
}

nBool InterpreterASTConsumer::HandleTopLevelDecl(NatsuLib::Linq<NatsuLib::Valued<Declaration::DeclPtr>> const& decls)
{
	nat_Throw(NotImplementedException);
}
