#ifndef DIAG
#define DIAG(ID, Level, ArgCount)
#endif

DIAG(ErrUndefinedIdentifier, Level::Error, 1)
DIAG(ErrMultiCharInLiteral, Level::Error, 0)
DIAG(ErrUnexpectEOF, Level::Error, 0)

#undef DIAG
