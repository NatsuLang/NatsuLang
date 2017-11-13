﻿#ifndef TOK
#define TOK(X)
#endif
#ifndef PUNCTUATOR
#define PUNCTUATOR(X,Y) TOK(X)
#endif
#ifndef KEYWORD
#define KEYWORD(X) TOK(Kw_ ## X)
#endif

TOK(Unknown)

TOK(Eof)
TOK(Comment)
TOK(CodeCompletion)

TOK(Identifier)

TOK(NumericLiteral)
TOK(CharLiteral)
TOK(StringLiteral)

PUNCTUATOR(LeftSquare,			"[")
PUNCTUATOR(RightSquare,			"]")
PUNCTUATOR(LeftParen,			"(")
PUNCTUATOR(RightParen,			")")
PUNCTUATOR(LeftBrace,			"{")
PUNCTUATOR(RightBrace,			"}")
PUNCTUATOR(Period,				".")
PUNCTUATOR(Ellipsis,			"...")
PUNCTUATOR(Amp,					"&")
PUNCTUATOR(AmpAmp,				"&&")
PUNCTUATOR(AmpEqual,			"&=")
PUNCTUATOR(Star,				"*")
PUNCTUATOR(StarEqual,			"*=")
PUNCTUATOR(Plus,				"+")
PUNCTUATOR(PlusPlus,			"++")
PUNCTUATOR(PlusEqual,			"+=")
PUNCTUATOR(Minus,				"-")
PUNCTUATOR(MinusMinus,			"--")
PUNCTUATOR(MinusEqual,			"-=")
PUNCTUATOR(Arrow,				"->")
PUNCTUATOR(Tilde,				"~")
PUNCTUATOR(Exclaim,				"!")
PUNCTUATOR(ExclaimEqual,		"!=")
PUNCTUATOR(Slash,				"/")
PUNCTUATOR(SlashEqual,			"/=")
PUNCTUATOR(Percent,				"%")
PUNCTUATOR(PercentEqual,		"%=")
PUNCTUATOR(Less,				"<")
PUNCTUATOR(LessLess,			"<<")
PUNCTUATOR(LessEqual,			"<=")
PUNCTUATOR(LessLessEqual,		"<<=")
PUNCTUATOR(Greater,				">")
PUNCTUATOR(GreaterGreater,		">>")
PUNCTUATOR(GreaterEqual,		">=")
PUNCTUATOR(GreaterGreaterEqual,	">>=")
PUNCTUATOR(Caret,				"^")
PUNCTUATOR(CaretEqual,			"^=")
PUNCTUATOR(Pipe,				"|")
PUNCTUATOR(PipePipe,			"||")
PUNCTUATOR(PipeEqual,			"|=")
PUNCTUATOR(Question,			"?")
PUNCTUATOR(Colon,				":")
PUNCTUATOR(Semi,				";")
PUNCTUATOR(Equal,				"=")
PUNCTUATOR(EqualEqual,			"==")
PUNCTUATOR(Comma,				",")
PUNCTUATOR(Hash,				"#")
PUNCTUATOR(Dollar,				"$")
PUNCTUATOR(At,					"@")

KEYWORD(if)
KEYWORD(else)
KEYWORD(for)
KEYWORD(while)
KEYWORD(def)
KEYWORD(auto)
KEYWORD(const)
KEYWORD(extern)
KEYWORD(static)
KEYWORD(public)
KEYWORD(protected)
KEYWORD(internal)
KEYWORD(private)
KEYWORD(class)
KEYWORD(module)
KEYWORD(template)
KEYWORD(return)
KEYWORD(import)
KEYWORD(goto)
KEYWORD(continue)
KEYWORD(break)
KEYWORD(try)
KEYWORD(catch)
KEYWORD(throw)
KEYWORD(true)
KEYWORD(false)
KEYWORD(this)
KEYWORD(typeof)
KEYWORD(as)

KEYWORD(void)
KEYWORD(bool)
KEYWORD(char)
KEYWORD(ushort)
KEYWORD(uint)
KEYWORD(ulong)
KEYWORD(ulonglong)
KEYWORD(uint128)
KEYWORD(short)
KEYWORD(int)
KEYWORD(long)
KEYWORD(longlong)
KEYWORD(int128)
KEYWORD(float)
KEYWORD(double)
KEYWORD(longdouble)
KEYWORD(float128)

#undef KEYWORD
#undef PUNCTUATOR
#undef TOK
