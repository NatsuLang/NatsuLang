#pragma once
#include "Basic/Diagnostic.h"

namespace NatsuLang::Lex
{
	class NumericLiteralParser
	{
	public:
		NumericLiteralParser(nStrView buffer, SourceLocation loc, Diag::DiagnosticsEngine& diag);

		nBool GetIntegerValue(nuLong& result) const noexcept;
		nBool GetFloatValue(nDouble& result) const noexcept;

	private:
		Diag::DiagnosticsEngine& m_Diag;
		nStrView m_Buffer;
		nStrView::iterator m_Current, m_DigitBegin, m_SuffixBegin;

		nBool m_SawPeriod, m_SawSuffix;

		nuInt m_Radix;
		
		nBool m_IsFloat : 1, m_IsUnsigned : 1, m_IsLong : 1;

		void parseNumberStartingWithZero(SourceLocation loc) noexcept;

		nStrView::iterator skipHexDigits(nStrView::iterator cur) const noexcept;
		nStrView::iterator skipOctalDigits(nStrView::iterator cur) const noexcept;
		nStrView::iterator skipDigits(nStrView::iterator cur) const noexcept;
		nStrView::iterator skipBinaryDigits(nStrView::iterator cur) const noexcept;
	};

	class CharLiteralParser
	{
	public:
		CharLiteralParser();

	private:

	};

	class StringLiteralParser
	{
	public:
		StringLiteralParser();

	private:

	};
}
