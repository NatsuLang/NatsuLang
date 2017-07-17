#pragma once

namespace NatsuLang::Specifier
{
	enum class StorageClass
	{
		None,
		Extern,
		Static,
	};

	enum class TypeSpecifier
	{
		None,

		Void,

		Bool,

		Char,

		UShort,
		UInt,
		ULong,
		ULongLong,
		UInt128,

		Short,
		Int,
		Long,
		LongLong,
		Int128,
		Float,
		Double,
		LongDouble,
		Float128,

		Enum,
		Class,
		Function,
		Array,
		TypeOf,
		Auto,

		Error
	};
}
