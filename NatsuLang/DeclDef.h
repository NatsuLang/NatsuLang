#ifndef DECL
#define DECL(Type, Base)
#endif
#ifndef ABSTRACT_DECL
#define ABSTRACT_DECL(Type) Type
#endif
#ifndef DECLRANGE
#define DECLRANGE(Base, First, Last)
#endif

#ifndef NAMED
#define NAMED(Type, Base) DECL(Type, Base)
#endif
ABSTRACT_DECL(NAMED(Named, Decl))

#ifndef LABEL
#define LABEL(Type, Base) NAMED(Type, Base)
#endif
LABEL(Label, NamedDecl)
#undef LABEL

#ifndef MODULE
#define MODULE(Type, Base) NAMED(Type, Base)
#endif
MODULE(Module, NamedDecl)
#undef MODULE

#ifndef TYPE
#define TYPE(Type, Base) NAMED(Type, Base)
#endif
ABSTRACT_DECL(TYPE(Type, NamedDecl))

#ifndef TAG
#define TAG(Type, Base) TYPE(Type, Base)
#endif
ABSTRACT_DECL(TAG(Tag, TypeDecl))

#ifndef ENUM
#define ENUM(Type, Base) TAG(Type, Base)
#endif
ENUM(Enum, TagDecl)
#undef ENUM


