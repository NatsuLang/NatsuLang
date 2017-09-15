#ifndef DECL
#define DECL(Type, Base)
#endif
#ifndef ABSTRACT_DECL
#define ABSTRACT_DECL(Type) Type
#endif
#ifndef DECL_RANGE
#define DECL_RANGE(Base, First, Last)
#endif

#ifndef LAST_DECL_RANGE
#define LAST_DECL_RANGE(Base, First, Last) DECL_RANGE(Base, First, Last)
#endif

#ifndef EMPTY
#define EMPTY(Type, Base) DECL(Type, Base)
#endif
EMPTY(Empty, Decl)
#undef EMPTY

#ifndef IMPORT
#define IMPORT(Type, Base) DECL(Type, Base)
#endif
IMPORT(Import, Decl)
#undef IMPORT

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

#ifndef RECORD
#define RECORD(Type, Base) TAG(Type, Base)
#endif
RECORD(Record, TagDecl)
#undef RECORD

DECL_RANGE(Tag, Enum, Record)

#undef TAG

#undef TYPE

#ifndef VALUE
#define VALUE(Type, Base) NAMED(Type, Base)
#endif
ABSTRACT_DECL(VALUE(Value, NamedDecl))

#ifndef DECLARATOR
#define DECLARATOR(Type, Base) VALUE(Type, Base)
#endif
ABSTRACT_DECL(DECLARATOR(Declarator, ValueDecl))

#ifndef FIELD
#define FIELD(Type, Base) DECLARATOR(Type, Base)
#endif
FIELD(Field, DeclaratorDecl)
#undef FIELD

#ifndef FUNCTION
#define FUNCTION(Type, Base) DECLARATOR(Type, Base)
#endif
FUNCTION(Function, DeclaratorDecl)

#ifndef METHOD
#define METHOD(Type, Base) FUNCTION(Type, Base)
#endif
METHOD(Method, FunctionDecl)

#ifndef CONSTRUCTOR
#define CONSTRUCTOR(Type, Base) METHOD(Type, Base)
#endif
CONSTRUCTOR(Constructor, MethodDecl)
#undef CONSTRUCTOR

#ifndef DESTRUCTOR
#define DESTRUCTOR(Type, Base) METHOD(Type, Base)
#endif
DESTRUCTOR(Destructor, MethodDecl)
#undef DESTRUCTOR

DECL_RANGE(Method, Method, Destructor)

#undef METHOD

DECL_RANGE(Function, Function, Destructor)

#undef FUNCTION

#ifndef VAR
#define VAR(Type, Base) DECLARATOR(Type, Base)
#endif
VAR(Var, DeclaratorDecl)

#ifndef IMPLICITPARAM
#  define IMPLICITPARAM(Type, Base) VAR(Type, Base)
#endif
IMPLICITPARAM(ImplicitParam, VarDecl)
#undef IMPLICITPARAM

#ifndef PARMVAR
#  define PARMVAR(Type, Base) VAR(Type, Base)
#endif
PARMVAR(ParmVar, VarDecl)
#undef PARMVAR

DECL_RANGE(Var, Var, ParmVar)

#undef VAR

DECL_RANGE(Declarator, Field, ParmVar)

#undef DECLARATOR

#ifndef ENUMCONSTANT
#define ENUMCONSTANT(Type, Base) VALUE(Type, Base)
#endif
ENUMCONSTANT(EnumConstant, ValueDecl)
#undef ENUMCONSTANT

DECL_RANGE(Value, Field, EnumConstant)

#undef VALUE

DECL_RANGE(Named, Label, EnumConstant)

#undef NAMED

#ifndef TRANSLATIONUNIT
#define TRANSLATIONUNIT(Type, Base) DECL(Type, Base)
#endif
TRANSLATIONUNIT(TranslationUnit, Decl)
#undef TRANSLATIONUNIT

LAST_DECL_RANGE(Decl, Empty, TranslationUnit)

#undef DECL
#undef DECL_RANGE
#undef LAST_DECL_RANGE
#undef ABSTRACT_DECL

#ifndef DECL_CONTEXT
#define DECL_CONTEXT(DECL)
#endif
#ifndef DECL_CONTEXT_BASE
#define DECL_CONTEXT_BASE(DECL) DECL_CONTEXT(DECL)
#endif
DECL_CONTEXT_BASE(Function)
DECL_CONTEXT_BASE(Tag)
DECL_CONTEXT(Module)
DECL_CONTEXT(TranslationUnit)
#undef DECL_CONTEXT
#undef DECL_CONTEXT_BASE
