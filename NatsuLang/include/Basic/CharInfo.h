#pragma once
#include <natMisc.h>
#include <natString.h>

namespace NatsuLang::CharInfo
{
	enum class CharInfo : nuShort
	{
		None		= 0x0000, // 无属性

		HorzWs		= 0x0001, // 横向空白字符
		VertWs		= 0x0002, // 纵向空白字符
		Space		= 0x0004, // 空格
		Digit		= 0x0008, // 数字
		XLetter		= 0x0010, // 十六进制使用字母
		Upper		= 0x0020, // 大写字母
		Lower		= 0x0040, // 小写字母
		Under		= 0x0080, // 下划线
		Period		= 0x0100, // 点
		Punct		= 0x0200, // 标点

		XUpper		= XLetter | Upper,
		XLower		= XLetter | Lower,
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(CharInfo);

	constexpr CharInfo CharInfoTable[256]
	{
		// 0 NUL         1 SOH         2 STX         3 ETX
		// 4 EOT         5 ENQ         6 ACK         7 BEL
		CharInfo::None, CharInfo::None, CharInfo::None, CharInfo::None, 
		CharInfo::None, CharInfo::None, CharInfo::None, CharInfo::None, 
		// 8 BS          9 HT         10 NL         11 VT
		//12 NP         13 CR         14 SO         15 SI
		CharInfo::None, CharInfo::HorzWs, CharInfo::VertWs, CharInfo::HorzWs,
		CharInfo::HorzWs, CharInfo::VertWs, CharInfo::None, CharInfo::None, 
		//16 DLE        17 DC1        18 DC2        19 DC3
		//20 DC4        21 NAK        22 SYN        23 ETB
		CharInfo::None, CharInfo::None, CharInfo::None, CharInfo::None, 
		CharInfo::None, CharInfo::None, CharInfo::None, CharInfo::None, 
		//24 CAN        25 EM         26 SUB        27 ESC
		//28 FS         29 GS         30 RS         31 US
		CharInfo::None, CharInfo::None, CharInfo::None, CharInfo::None, 
		CharInfo::None, CharInfo::None, CharInfo::None, CharInfo::None, 
		//32 SP         33  !         34  "         35  #
		//36  $         37  %         38  &         39  '
		CharInfo::Space, CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, 
		CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, 
		//40  (         41  )         42  *         43  +
		//44 ,         45  -         46  .         47  /
		CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, 
		CharInfo::Punct, CharInfo::Punct, CharInfo::Period, CharInfo::Punct, 
		//48  0         49  1         50  2         51  3
		//52  4         53  5         54  6         55  7
		CharInfo::Digit, CharInfo::Digit, CharInfo::Digit, CharInfo::Digit, 
		CharInfo::Digit, CharInfo::Digit, CharInfo::Digit, CharInfo::Digit, 
		//56  8         57  9         58  :         59  ;
		//60  <         61  =         62  >         63  ?
		CharInfo::Digit, CharInfo::Digit, CharInfo::Punct, CharInfo::Punct, 
		CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, 
		//64  @         65  A         66  B         67  C
		//68  D         69  E         70  F         71  G
		CharInfo::Punct, CharInfo::XUpper, CharInfo::XUpper, CharInfo::XUpper, 
		CharInfo::XUpper, CharInfo::XUpper, CharInfo::XUpper, CharInfo::Upper, 
		//72  H         73  I         74  J         75  K
		//76  L         77  M         78  N         79  O
		CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, 
		CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, 
		//80  P         81  Q         82  R         83  S
		//84  T         85  U         86  V         87  W
		CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, 
		CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, 
		//88  X         89  Y         90  Z         91  [
		//92  \         93  ]         94  ^         95  _
		CharInfo::Upper, CharInfo::Upper, CharInfo::Upper, CharInfo::Punct, 
		CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, CharInfo::Under, 
		//96  `         97  a         98  b         99  c
		//100  d       101  e        102  f        103  g
		CharInfo::Punct, CharInfo::XLower, CharInfo::XLower, CharInfo::XLower, 
		CharInfo::XLower, CharInfo::XLower, CharInfo::XLower, CharInfo::Lower, 
		//104  h       105  i        106  j        107  k
		//108  l       109  m        110  n        111  o
		CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, 
		CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, 
		//112  p       113  q        114  r        115  s
		//116  t       117  u        118  v        119  w
		CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, 
		CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, 
		//120  x       121  y        122  z        123  {
		//124  |       125  }        126  ~        127 DEL
		CharInfo::Lower, CharInfo::Lower, CharInfo::Lower, CharInfo::Punct, 
		CharInfo::Punct, CharInfo::Punct, CharInfo::Punct, CharInfo::None
	};

	constexpr bool IsAscii(char c) noexcept
	{
		return static_cast<unsigned char>(c) < 127;
	}

	constexpr bool IsIdentifierHead(unsigned char c, unsigned char allowLeadingChar = 0) noexcept
	{
		if ((CharInfoTable[c] & (CharInfo::Upper | CharInfo::Lower | CharInfo::Under)) != CharInfo::None)
		{
			return true;
		}

		return allowLeadingChar && allowLeadingChar == c;
	}

	constexpr bool IsIdentifierBody(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::Upper | CharInfo::Lower | CharInfo::Under | CharInfo::Digit)) != CharInfo::None;
	}

	constexpr bool IsHorizontalWhitespace(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::HorzWs | CharInfo::Space)) != CharInfo::None;
	}

	constexpr bool IsVerticalWhitespace(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & CharInfo::VertWs) != CharInfo::None;
	}

	constexpr bool IsWhitespace(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::HorzWs | CharInfo::VertWs | CharInfo::Space)) != CharInfo::None;
	}

	constexpr bool IsDigit(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & CharInfo::Digit) != CharInfo::None;
	}

	constexpr bool IsLowerCase(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & CharInfo::Lower) != CharInfo::None;
	}

	constexpr bool IsUpperCase(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & CharInfo::Upper) != CharInfo::None;
	}

	constexpr bool IsLetter(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::Upper | CharInfo::Lower)) != CharInfo::None;
	}

	constexpr bool IsAlphanumeric(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::Upper | CharInfo::Lower | CharInfo::Digit)) != CharInfo::None;
	}

	constexpr bool IsHexDigit(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::XLetter | CharInfo::Digit)) != CharInfo::None;
	}

	constexpr bool IsPunctuation(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::Under | CharInfo::Period | CharInfo::Punct)) != CharInfo::None;
	}

	constexpr bool IsPrintable(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::Under | CharInfo::Lower | CharInfo::Period | CharInfo::Punct |
			CharInfo::Digit | CharInfo::Under | CharInfo::Space)) != CharInfo::None;
	}

	constexpr bool IsNumericLiteralBody(unsigned char c) noexcept
	{
		return (CharInfoTable[c] & (CharInfo::Upper | CharInfo::Lower | CharInfo::Digit | CharInfo::Period)) != CharInfo::None;
	}

	constexpr char ToLowerCase(char c) noexcept
	{
		if (IsUpperCase(c))
		{
			return c - 'A' + 'a';
		}
		return c;
	}

	constexpr char ToUpperCase(char c) noexcept
	{
		if (IsLowerCase(c))
		{
			return c - 'a' + 'A';
		}
		return c;
	}

	inline bool IsValidIdentifier(nStrView str) noexcept
	{
		if (str.empty())
		{
			return false;
		}

		auto i = str.cbegin();
		const auto end = str.cend();

		if (!IsIdentifierHead(*i))
		{
			return false;
		}

		while (++i != end)
		{
			if (!IsIdentifierBody(*i))
			{
				return false;
			}
		}

		return true;
	}
}
