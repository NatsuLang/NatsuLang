#include "Basic/Module.h"
#include "AST/Expression.h"
#include "AST/NestedNameSpecifier.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

Module::Module(nString name, SourceLocation definitionLocation, natWeakRefPointer<Module> parent)
	: m_Name{ std::move(name) }, m_DefinitionLocation{ definitionLocation }
{
}

Module::~Module()
{
}
