#pragma once
#include <natRefObj.h>
#include <natString.h>
#include <natLinq.h>
#include "SourceLocation.h"

namespace NatsuLang::Declaration
{
	class ModuleDecl;
}

namespace NatsuLang
{
	class Module
		: public NatsuLib::natRefObjImpl<Module>
	{
	public:
		Module(nString name, SourceLocation definitionLocation, NatsuLib::natWeakRefPointer<Module> parent = {});
		~Module();

		// TODO
		

	private:
		nString m_Name;
		SourceLocation m_DefinitionLocation;
		NatsuLib::natRefPointer<Declaration::ModuleDecl> m_Module;
	};
}
