#include "Lex/LiteralParser.h"
#include "Basic/CharInfo.h"
#include "Basic/Identifier.h"

#undef min
#undef max

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Lex;

namespace
{
	template <typename Target, typename Origin>
	constexpr nBool InTypeRangeImpl(Origin value, std::true_type) noexcept
	{
		return value >= static_cast<Origin>(std::numeric_limits<Target>::min()) && value <= static_cast<Origin>(std::numeric_limits<Target>::max());
	}

	template <typename Target, typename Origin>
	constexpr nBool InTypeRangeImpl(Origin, std::false_type) noexcept
	{
		return true;
	}

	template <typename Target, typename Origin, std::enable_if_t<std::is_arithmetic_v<Target> && std::is_arithmetic_v<Origin>, nuInt> = 0>
	constexpr nBool InTypeRange(Origin value) noexcept
	{
		return InTypeRangeImpl<Target>(value, std::bool_constant<(sizeof(Target) < sizeof(Origin))>{});
	}

	constexpr nuInt DigitValue(nuInt ch) noexcept
	{
		assert(ch >= static_cast<nuInt>(std::numeric_limits<unsigned char>::min()) && ch <= static_cast<nuInt>(std::numeric_limits<unsigned char>::max()));
		assert(InTypeRange<unsigned char>(ch));
		const auto c = static_cast<unsigned char>(ch);
		assert(NatsuLang::CharInfo::IsAlphanumeric(c));

		if (NatsuLang::CharInfo::IsDigit(c))
		{
			return static_cast<nuInt>(c - unsigned char{ '0' });
		}

		if (c > 'a')
		{
			return static_cast<nuInt>(c - unsigned char{ 'a' } + 10);
		}

		if (c > 'A')
		{
			return static_cast<nuInt>(c - unsigned char{ 'A' } + 10);
		}

		// 错误，这个字符不是可用的数字字面量字符
		return 0;
	}

	nuInt EscapeChar(nString::const_iterator& cur, nString::const_iterator end, nBool& errored, NatsuLang::SourceLocation loc, NatsuLang::Diag::DiagnosticsEngine& diag) noexcept
	{
		assert(*cur == '\\');

		++cur;
		nuInt chr = *cur++;
		switch (chr)
		{
		case '\'':
		case '\\':
		case '"':
		case '?':
			break;
		case 'a':
			// TODO: 参考标准替换为实际数值
			chr = '\a';
			break;
		case 'b':
			chr = '\b';
			break;
		case 'f':
			chr = '\f';
			break;
		case 'n':
			chr = '\n';
			break;
		case 'r':
			chr = '\r';
			break;
		case 't':
			chr = '\t';
			break;
		case 'v':
			chr = '\v';
			break;
		case 'x':
		{
			auto overflowed = false;

			chr = 0;
			for (; cur != end; ++cur)
			{
				const auto curChr = *cur;
				const auto curValue = DigitValue(static_cast<nuInt>(curChr));
				if (chr & 0xF0000000)
				{
					overflowed = true;
				}
				chr <<= 4;
				chr |= curValue;
			}

			// TODO: 适配具有更多宽度的字符
			if (chr >> 8)
			{
				overflowed = true;
				chr &= ~0u >> 24; // 32 - 8
			}

			if (overflowed)
			{
				// TODO: 报告溢出
			}

			break;
		}
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
		{
			--cur;

			chr = 0;
			nuInt charCount = 0;
			do
			{
				chr <<= 3;
				chr |= DigitValue(*cur++);
				++charCount;
			} while (cur != end && charCount < 3 && *cur >= '0' && *cur <= '7');

			// TODO: 适配具有更多宽度的字符
			if (chr >> 8)
			{
				chr &= ~0u >> 24; // 32 - 8
								  // TODO: 报告溢出
			}

			break;
		}
		default:
			break;
		}

		return chr;
	}
}

NumericLiteralParser::NumericLiteralParser(nStrView buffer, SourceLocation loc, Diag::DiagnosticsEngine& diag)
	: m_Diag{ diag },
	m_Buffer{ buffer },
	m_Current{ m_Buffer.cbegin() },
	m_DigitBegin{ m_Current }, m_SuffixBegin{},
	m_SawPeriod{ false }, m_SawSuffix{ false },
	m_Radix{ 10 },
	m_Errored{ false },
	m_IsFloat{ false }, m_IsUnsigned{ false }, m_IsLong{ false }, m_IsLongLong{ false }
{
	if (*m_Current == '0')
	{
		parseNumberStartingWithZero(loc);
	}
	else
	{
		m_Radix = 10;
		m_Current = skipDigits(m_Current);

		// TODO: 完成读取指数与小数部分
	}

	// TODO: 读取后缀
	m_SuffixBegin = m_Current;
	const auto end = m_Buffer.cend();
	for (; m_Current != end; ++m_Current)
	{
		// 错误时跳过所有后缀
		if (m_Errored)
		{
			continue;
		}

		switch (*m_Current)
		{
		case 'f':
		case 'F':
			m_IsFloat = true;
			// 浮点数
			break;
		case 'u':
		case 'U':
			m_IsUnsigned = true;
			// 无符号数
			break;
		case 'l':
		case 'L':
			if (m_Current + 1 < end && m_Current[1] == m_Current[0])
			{
				m_IsLongLong = true;
				++m_Current;
			}
			else
			{
				m_IsLong = true;
			}

			// 长整数或长浮点数
			break;
		default:
			// 错误：无效的后缀
			// TODO: 记录当前位置以进行错误报告
			m_Errored = true;
			break;
		}
	}
}

nBool NumericLiteralParser::GetIntegerValue(nuLong& result) const noexcept
{
	for (auto i = m_DigitBegin; i != m_SuffixBegin; ++i)
	{
		result = result * m_Radix + DigitValue(*i);
	}

	// TODO: 检查是否溢出
	return false;
}

nBool NumericLiteralParser::GetFloatValue(nDouble& result) const noexcept
{
	nStrView::iterator periodPos{};
	nDouble partAfterPeriod{};
	for (auto i = m_DigitBegin; i != m_SuffixBegin; ++i)
	{
		if (!periodPos)
		{
			if (*i == '.')
			{
				periodPos = i;
				continue;
			}
			result = result * m_Radix + DigitValue(*i);
		}
		else
		{
			partAfterPeriod = partAfterPeriod * m_Radix + DigitValue(*i);
		}
	}

	if (periodPos)
	{
		result = partAfterPeriod * pow(m_Radix, periodPos - m_SuffixBegin);
	}
	
	// TODO: 检查是否溢出
	return false;
}

void NumericLiteralParser::parseNumberStartingWithZero(SourceLocation loc) noexcept
{
	assert(*m_Current == '0');
	++m_Current;

	const auto cur = *m_Current, next = *(m_Current + 1);
	if ((cur == 'x' || cur == 'X') && (CharInfo::IsHexDigit(next) || next == '.'))
	{
		++m_Current;
		m_Radix = 16;
		m_DigitBegin = m_Current;
		m_Current = skipHexDigits(m_Current);

		if (*m_Current == '.')
		{
			++m_Current;
			m_SawPeriod = true;
			m_Current = skipHexDigits(m_Current);
		}

		// TODO: 十六进制的浮点数字面量

		return;
	}

	if ((cur == 'b' || cur == 'B') && (next == '0' || next == '1'))
	{
		++m_Current;
		m_Radix = 2;
		m_DigitBegin = m_Current;
		m_Current = skipBinaryDigits(m_Current);
		return;
	}

	// 0开头后面没有跟xXbB字符的视为八进制数字面量
	m_Radix = 8;
	m_DigitBegin = m_Current;
	m_Current = skipOctalDigits(m_Current);

	// TODO: 八进制的浮点数字面量
}

nStrView::iterator NumericLiteralParser::skipHexDigits(nStrView::iterator cur) const noexcept
{
	const auto end = m_Buffer.end();
	while (cur != end && CharInfo::IsHexDigit(*cur))
	{
		++cur;
	}

	return cur;
}

nStrView::iterator NumericLiteralParser::skipOctalDigits(nStrView::iterator cur) const noexcept
{
	const auto end = m_Buffer.end();
	while (cur != end && (*cur >= '0' && *cur <= '7'))
	{
		++cur;
	}

	return cur;
}

nStrView::iterator NumericLiteralParser::skipDigits(nStrView::iterator cur) const noexcept
{
	const auto end = m_Buffer.end();
	while (cur != end && CharInfo::IsDigit(*cur))
	{
		++cur;
	}

	return cur;
}

nStrView::iterator NumericLiteralParser::skipBinaryDigits(nStrView::iterator cur) const noexcept
{
	const auto end = m_Buffer.end();
	while (cur != end && (*cur >= '0' && *cur <= '1'))
	{
		++cur;
	}

	return cur;
}

CharLiteralParser::CharLiteralParser(nStrView buffer, SourceLocation loc, Diag::DiagnosticsEngine& diag)
	: m_Diag{ diag }, m_Buffer{ buffer }, m_Current{ buffer.cbegin() }, m_Value{}, m_Errored{ false }
{
	assert(m_Buffer.GetSize() > 2);
	assert(*m_Current == '\'');
	++m_Current;
	assert(buffer.cend()[-1] == '\'');
	const auto end = std::prev(buffer.cend());
	if (*m_Current == '\\')
	{
		m_Value = EscapeChar(m_Current, m_Buffer.cend(), m_Errored, loc, m_Diag);
	}
	else
	{
		const auto charCount = StringEncodingTrait<nString::UsingStringType>::GetCharCount(*m_Current);
		if (charCount != 1)
		{
			m_Errored = true;
			// TODO: 提示单个char无法容纳该字符值
		}
		else
		{
			const auto bufferLength = static_cast<std::size_t>(std::distance(m_Current, end));
			if (charCount < bufferLength)
			{
				m_Errored = true;
				m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrMultiCharInLiteral, loc);
			}
			else if (charCount > bufferLength)
			{
				m_Errored = true;
				// TODO: 报告字符未能完整地存储在字符字面量中
			}
			else
			{
				U32String tmpStr = nStrView{ m_Current, end };
				assert(tmpStr.size() == 1);
				m_Value = static_cast<nuInt>(tmpStr[0]);
			}
		}
	}
}

StringLiteralParser::StringLiteralParser(nStrView buffer, SourceLocation loc, Diag::DiagnosticsEngine& diag)
	: m_Diag{ diag }, m_Buffer{ buffer }, m_Current{ buffer.cbegin() }, m_Errored{ false }
{
	assert(m_Buffer.GetSize() >= 2);
	assert(*m_Current == '"');
	++m_Current;
	assert(buffer.cend()[-1] == '"');
	const auto end = std::prev(buffer.cend());

	m_Value.Reserve(m_Buffer.GetSize() - 2);

	for (; m_Current != end; ++m_Current)
	{
		if (*m_Current != '\\')
		{
			if (*m_Current == '"')
			{
				// TODO: 报告字符串过早结束
				break;
			}

			m_Value.Append(*m_Current);
		}
		else
		{
			auto codePoint = EscapeChar(m_Current, end, m_Errored, loc, m_Diag);
			// TODO: 适配具有不同宽度的字符串
			m_Value.Append(static_cast<nString::CharType>(codePoint & std::numeric_limits<nString::CharType>::max()));
		}
	}
}
