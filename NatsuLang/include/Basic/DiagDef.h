#ifndef DIAG
#define DIAG(ID, Level, ArgCount)
#endif

DIAG(ErrUndefinedIdentifier, Level::Error, 1)
DIAG(ErrMultiCharInLiteral, Level::Error, 0)
DIAG(ErrUnexpectEOF, Level::Error, 0)
DIAG(ErrUnexpect, Level::Error, 1)
DIAG(ErrExpectedIdentifier, Level::Error, 0)
DIAG(ErrExpectedTypeSpecifierGot, Level::Error, 1)
DIAG(ErrExpectedDeclarator, Level::Error, 0)
DIAG(ErrExpected, Level::Error, 1)
DIAG(ErrExpectedGot, Level::Error, 2)
DIAG(ErrExtraneousClosingBrace, Level::Error, 0)
DIAG(ErrNotAllControlFlowReturnAValue, Level::Error, 0)
DIAG(ErrUnsafeOperationInSafeScope, Level::Error, 0)
DIAG(ErrDuplicateDeclaration, Level::Error, 1)
DIAG(ErrExpressionCannotEvaluateAsConstant, Level::Error, 0)

DIAG(WarnOverflowed, Level::Warning, 0)

DIAG(NoteSee, Level::Note, 0)

#undef DIAG
