#include "Sema/Sema.h"
#include "Lex/Preprocessor.h"

using namespace NatsuLang::Semantic;

Sema::Sema(Preprocessor& preprocessor)
	: m_Preprocessor{ preprocessor }, m_Diag{ preprocessor.GetDiag() }, m_SourceManager{ preprocessor.GetSourceManager() }
{
}

Sema::~Sema()
{
}

NatsuLib::natRefPointer<NatsuLang::Declaration::Decl> Sema::OnModuleImport(SourceLocation startLoc, SourceLocation importLoc, ModulePathType const& path)
{
	nat_Throw(NatsuLib::NotImplementedException);
}

NatsuLang::Type::TypePtr Sema::GetTypeName(NatsuLib::natRefPointer<Identifier::IdentifierInfo> const& id, SourceLocation nameLoc, NatsuLib::natRefPointer<Scope> scope)
{

}
