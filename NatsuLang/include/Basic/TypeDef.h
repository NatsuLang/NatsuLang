#ifndef TYPE
#define TYPE(Class, Base)
#endif

#ifndef ABSTRACT_TYPE
#define ABSTRACT_TYPE(Class, Base) TYPE(Class, Base)
#endif

#ifndef NON_CANONICAL_TYPE
#define NON_CANONICAL_TYPE(Class, Base) TYPE(Class, Base)
#endif

#ifndef DEPENDENT_TYPE
#define DEPENDENT_TYPE(Class, Base) TYPE(Class, Base)
#endif

#ifndef NON_CANONICAL_UNLESS_DEPENDENT_TYPE
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(Class, Base) TYPE(Class, Base)
#endif

TYPE(Builtin, Type)
TYPE(Pointer, Type)
TYPE(Array, Type)
TYPE(Function, Type)
NON_CANONICAL_TYPE(Paren, Type)
NON_CANONICAL_UNLESS_DEPENDENT_TYPE(TypeOf, Type)
ABSTRACT_TYPE(Tag, Type)
TYPE(Class, TagType)
TYPE(Enum, TagType)
ABSTRACT_TYPE(Deduced, Type)
TYPE(Auto, DeducedType)
TYPE(Unresolved, Type)

#ifdef LAST_TYPE
LAST_TYPE(Unresolved)
#undef LAST_TYPE
#endif

#ifdef LEAF_TYPE
LEAF_TYPE(Enum)
LEAF_TYPE(Builtin)
LEAF_TYPE(Class)
#undef LEAF_TYPE
#endif

#undef NON_CANONICAL_UNLESS_DEPENDENT_TYPE
#undef DEPENDENT_TYPE
#undef NON_CANONICAL_TYPE
#undef ABSTRACT_TYPE
#undef TYPE
