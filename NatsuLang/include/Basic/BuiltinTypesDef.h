#ifndef BUILTIN_TYPE
#define BUILTIN_TYPE(Id, Name)
#endif

#ifndef SIGNED_TYPE
#define SIGNED_TYPE(Id, Name) BUILTIN_TYPE(Id, Name)
#endif

#ifndef UNSIGNED_TYPE
#define UNSIGNED_TYPE(Id, Name) BUILTIN_TYPE(Id, Name)
#endif

#ifndef FLOATING_TYPE
#define FLOATING_TYPE(Id, Name) BUILTIN_TYPE(Id, Name)
#endif

#ifndef PLACEHOLDER_TYPE
#define PLACEHOLDER_TYPE(Id, Name) BUILTIN_TYPE(Id, Name)
#endif

BUILTIN_TYPE(Void, void)
UNSIGNED_TYPE(Bool, bool)
UNSIGNED_TYPE(Char, char)
UNSIGNED_TYPE(Byte, byte)
UNSIGNED_TYPE(UShort, ushort)
UNSIGNED_TYPE(UInt, uint)
UNSIGNED_TYPE(ULong, ulong)
UNSIGNED_TYPE(ULongLong, ulonglong)
UNSIGNED_TYPE(UInt128, uint128)
SIGNED_TYPE(SByte, sbyte)
SIGNED_TYPE(Short, short)
SIGNED_TYPE(Int, int)
SIGNED_TYPE(Long, long)
SIGNED_TYPE(LongLong, longlong)
SIGNED_TYPE(Int128, int128)
FLOATING_TYPE(Float, float)
FLOATING_TYPE(Double, double)
FLOATING_TYPE(LongDouble, longdouble)
FLOATING_TYPE(Float128, float128)

PLACEHOLDER_TYPE(Null, null)
PLACEHOLDER_TYPE(Overload, overload)

#ifdef LAST_BUILTIN_TYPE
LAST_BUILTIN_TYPE(Overload)
#undef LAST_BUILTIN_TYPE
#endif

#undef PLACEHOLDER_TYPE
#undef FLOATING_TYPE
#undef SIGNED_TYPE
#undef UNSIGNED_TYPE
#undef BUILTIN_TYPE
