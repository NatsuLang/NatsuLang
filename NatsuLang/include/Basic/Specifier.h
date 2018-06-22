#pragma once
#include <natMisc.h>

namespace NatsuLang::Specifier
{
	enum class StorageClass
	{
		None	= 0x00,

		Extern	= 0x01,
		Static	= 0x02,
		Const	= 0x04,
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(StorageClass);

	enum class Access
	{
		None,

		Public,
		Protected,
		Internal,
		Private
	};

	enum class Safety
	{
		None,

		Unsafe
	};
}
