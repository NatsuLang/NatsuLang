#pragma once

namespace NatsuLang::Specifier
{
	enum class StorageClass
	{
		None,

		Extern,
		Static,
	};

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
