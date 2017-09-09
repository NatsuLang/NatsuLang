#include "Basic/Module.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Module;

NatsuLang::Module::Module::Module(nString name, SourceLocation DefinitionLocation, natWeakRefPointer<Module> parent)
	: m_Name{ std::move(name) }, m_DefinitionLocation{ DefinitionLocation }, m_Parent{ std::move(parent) }
{
}

NatsuLang::Module::Module::~Module()
{
}
