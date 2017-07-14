#ifndef BUILTIN_TYPE
#define BUILTIN_TYPE(Id, SingletonId, Name)
#endif

#ifndef SIGNED_TYPE
#define SIGNED_TYPE(Id, SingletonId, Name) BUILTIN_TYPE(Id, SingletonId, Name)
#endif

#ifndef UNSIGNED_TYPE
#define UNSIGNED_TYPE(Id, SingletonId, Name) BUILTIN_TYPE(Id, SingletonId, Name)
#endif

#ifndef FLOATING_TYPE
#define FLOATING_TYPE(Id, SingletonId, Name) BUILTIN_TYPE(Id, SingletonId, Name)
#endif

#ifndef PLACEHOLDER_TYPE
#define PLACEHOLDER_TYPE(Id, SingletonId, Name) BUILTIN_TYPE(Id, SingletonId, Name)
#endif

BUILTIN_TYPE(Void, VoidTy, void)
UNSIGNED_TYPE(Bool, BoolTy, bool)
UNSIGNED_TYPE(Char, CharTy, char)
UNSIGNED_TYPE(UShort, UnsignedShortTy, ushort)
UNSIGNED_TYPE(UInt, UnsignedIntTy, uint)
UNSIGNED_TYPE(ULong, UnsignedLongTy, ulong)
UNSIGNED_TYPE(ULongLong, UnsignedLongLongTy, ulonglong)
UNSIGNED_TYPE(UInt128, UnsignedInt128Ty, uint128)
SIGNED_TYPE(Short, ShortTy, short)
SIGNED_TYPE(Int, IntTy, int)
SIGNED_TYPE(Long, LongTy, long)
SIGNED_TYPE(LongLong, LongLongTy, longlong)
SIGNED_TYPE(Int128, Int128Ty, int128)
FLOATING_TYPE(Float, FloatTy, float)
FLOATING_TYPE(Double, DoubleTy, double)
FLOATING_TYPE(LongDouble, LongDoubleTy, longdouble)
FLOATING_TYPE(Float128, Float128Ty, float128)

PLACEHOLDER_TYPE(Overload, OverloadTy, overload)
PLACEHOLDER_TYPE(BoundMember, BoundMemberTy, boundmember)
PLACEHOLDER_TYPE(BuiltinFn, BuiltinFnTy, builtinfn)

#ifdef LAST_BUILTIN_TYPE
LAST_BUILTIN_TYPE(BuiltinFn)
#undef LAST_BUILTIN_TYPE
#endif

#undef PLACEHOLDER_TYPE
#undef FLOATING_TYPE
#undef SIGNED_TYPE
#undef UNSIGNED_TYPE
#undef BUILTIN_TYPE
