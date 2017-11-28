#pragma once
#include <natRefObj.h>

namespace NatsuLang
{
	class Metadata
	{
	public:
		Metadata();
		~Metadata();

	private:
	};

	struct MetadataSerializer
		: NatsuLib::natRefObj
	{
		virtual ~MetadataSerializer();


	};
}
