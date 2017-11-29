#include "Basic/Module.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Module;

NatsuLang::Module::Module::Module(nString name, SourceLocation definitionLocation, natWeakRefPointer<Module> parent)
	: m_Name{ std::move(name) }, m_DefinitionLocation{ definitionLocation }, m_Parent{ std::move(parent) }
{
}

NatsuLang::Module::Module::~Module()
{
}
