#pragma once
#include <natRefObj.h>
#include "SourceLocation.h"

namespace NatsuLang::Module
{
	class Module
		: public NatsuLib::natRefObjImpl<Module>
	{
	public:
		Module(nString name, SourceLocation DefinitionLocation, NatsuLib::natWeakRefPointer<Module> parent = {});
		~Module();

		// TODO

	private:
		nString m_Name;
		SourceLocation m_DefinitionLocation;
		NatsuLib::natWeakRefPointer<Module> m_Parent;
		std::vector<NatsuLib::natRefPointer<Module>> m_SubModules;
	};
}
