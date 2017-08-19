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

		nBool Errored() const noexcept
		{
			return m_Errored;
		}

		nBool IsIntegerLiteral() const noexcept
		{
			return !m_SawPeriod && !IsFloat();
		}

		nBool IsFloatingLiteral() const noexcept
		{
			return !IsIntegerLiteral();
		}

		nBool IsFloat() const noexcept
		{
			return m_IsFloat;
		}

		nBool IsUnsigned() const noexcept
		{
			return m_IsUnsigned;
		}

		nBool IsLong() const noexcept
		{
			return m_IsLong;
		}

		nBool IsLongLong() const noexcept
		{
			return m_IsLongLong;
		}

	private:
		Diag::DiagnosticsEngine& m_Diag;
		nStrView m_Buffer;
		nStrView::iterator m_Current, m_DigitBegin, m_SuffixBegin;

		nBool m_SawPeriod, m_SawSuffix;

		nuInt m_Radix;
		
		nBool m_Errored : 1, m_IsFloat : 1, m_IsUnsigned : 1, m_IsLong : 1, m_IsLongLong : 1;

		void parseNumberStartingWithZero(SourceLocation loc) noexcept;

		nStrView::iterator skipHexDigits(nStrView::iterator cur) const noexcept;
		nStrView::iterator skipOctalDigits(nStrView::iterator cur) const noexcept;
		nStrView::iterator skipDigits(nStrView::iterator cur) const noexcept;
		nStrView::iterator skipBinaryDigits(nStrView::iterator cur) const noexcept;
	};

	class CharLiteralParser
	{
	public:
		CharLiteralParser(nStrView buffer, SourceLocation loc, Diag::DiagnosticsEngine& diag);

		nuInt GetValue() const noexcept
		{
			return m_Value;
		}

		nBool Errored() const noexcept
		{
			return m_Errored;
		}

	private:
		Diag::DiagnosticsEngine& m_Diag;
		nStrView m_Buffer;
		nStrView::iterator m_Current;

		nuInt m_Value;
		nBool m_Errored;
	};

	class StringLiteralParser
	{
	public:
		StringLiteralParser(nStrView buffer, SourceLocation loc, Diag::DiagnosticsEngine& diag);

		nStrView GetValue() const noexcept
		{
			return m_Value;
		}

		nBool Errored() const noexcept
		{
			return m_Errored;
		}

	private:
		Diag::DiagnosticsEngine& m_Diag;
		nStrView m_Buffer;
		nStrView::iterator m_Current;

		nString m_Value;
		nBool m_Errored;
	};
}
