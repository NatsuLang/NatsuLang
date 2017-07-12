#include "AST/Declaration.h"
#include "Basic/Identifier.h"

using namespace NatsuLang::Declaration;

NamedDecl::~NamedDecl()
{
}

nStrView NamedDecl::GetName() const noexcept
{
	return m_IdentifierInfo->GetName();
}

ModuleDecl::~ModuleDecl()
{
}

ValueDecl::~ValueDecl()
{
}

DeclaratorDecl::~DeclaratorDecl()
{
}

VarDecl::~VarDecl()
{
}

ImplicitParamDecl::~ImplicitParamDecl()
{
}

ParmVarDecl::~ParmVarDecl()
{
}

FunctionDecl::~FunctionDecl()
{
}

NatsuLib::Linq<NatsuLib::natRefPointer<ParmVarDecl>> FunctionDecl::GetParams() const noexcept
{
	return from(m_Params);
}

void FunctionDecl::SetParams(NatsuLib::Linq<NatsuLib::natRefPointer<ParmVarDecl>> value) noexcept
{
	m_Params.assign(value.begin(), value.end());
}

FieldDecl::~FieldDecl()
{
}

EnumConstantDecl::~EnumConstantDecl()
{
}

TypeDecl::~TypeDecl()
{
}

TagDecl::~TagDecl()
{
}

EnumDecl::~EnumDecl()
{
}
