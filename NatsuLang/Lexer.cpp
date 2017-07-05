#include "Lexer.h"
#include "CharInfo.h"
#include "Preprocessor.h"

using namespace NatsuLang::Lex;
using namespace NatsuLang::Token;
using namespace NatsuLang::CharInfo;

Lexer::Lexer(nStrView buffer, Preprocessor& preprocessor)
	: m_Preprocessor{ preprocessor }, m_Buffer{ std::move(buffer) }, m_Current{ m_Buffer.cbegin() }
{
	if (m_Buffer.empty())
	{
		nat_Throw(LexerException, "buffer is empty."_nv);
	}
}

bool Lexer::Lex(Token::Token& result)
{
NextToken:
	result.Reset();

	auto cur = m_Current;
	const auto end = m_Buffer.end();

	// 跳过空白字符
	while (cur != end && IsWhitespace(*cur))
	{
		++cur;
	}

	if (cur == end)
	{
		return false;
	}

	const auto charCount = NatsuLib::StringEncodingTrait<nStrView::UsingStringType>::GetCharCount(*cur);
	if (charCount == 1)
	{
		switch (*cur)
		{
		case 0:
			result.SetType(TokenType::Eof);
			return true;
		case '\n':
		case '\r':
		case ' ':
		case '\t':
		case '\f':
		case '\v':
			if (skipWhitespace(result, cur))
			{
				return true;
			}
			goto NextToken;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			return lexNumericLiteral(result, cur);
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
		case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
		case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
		case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
		case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
		case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
		case 'v': case 'w': case 'x': case 'y': case 'z':
		case '_':
			return lexIdentifier(result, cur);
		case '\'':
			break;
		default:
			break;
		}
	}
	else
	{
		// TODO: ???
	}
	cur += charCount;

	m_Current = cur;
	return false;
}

bool Lexer::skipWhitespace(Token::Token& result, Iterator cur)
{
	const auto end = m_Buffer.end();

	while (cur != end && IsWhitespace(*cur))
	{
		++cur;
	}

	// TODO: 记录列信息

	m_Current = cur;
	return false;
}

bool Lexer::skipLineComment(Token::Token& result, Iterator cur)
{
	const auto end = m_Buffer.end();

	while (cur != end && *cur != '\r' && *cur != '\n')
	{
		++cur;
	}

	m_Current = cur;
	return false;
}

bool Lexer::skipBlockComment(Token::Token& result, Iterator cur)
{
	const auto end = m_Buffer.end();

	while (cur != end)
	{
		if (*cur == '*' && *(cur + 1) == '/')
		{
			cur += 2;
			break;
		}
		++cur;
	}

	m_Current = cur;
	return false;
}

bool Lexer::lexNumericLiteral(Token::Token& result, Iterator cur)
{
	CharType curChar = *cur, prevChar{};
	while (IsNumericLiteralBody(curChar))
	{
		prevChar = curChar;
		curChar = *cur++;
	}

	// 科学计数法，例如1E+10
	if ((curChar == '+' || curChar == '-') || (prevChar == 'e' || prevChar == 'E'))
	{
		return lexNumericLiteral(result, ++cur);
	}

	result.SetType(TokenType::NumericLiteral);
	result.SetLiteralContent({ m_Current, cur });
	m_Current = cur;
	return true;
}

bool Lexer::lexIdentifier(Token::Token& result, Iterator cur)
{
	const auto start = cur;
	auto curChar = *cur++;

	while (IsIdentifierBody(curChar))
	{
		curChar = *cur++;
	}

	m_Current = cur;

	auto info = m_Preprocessor.FindIdentifierInfo(nStrView{ start, curChar }, result);
	// 不需要对info进行操作，因为已经在FindIdentifierInfo中处理完毕
	static_cast<void>(info);

	return true;
}

bool Lexer::lexCharLiteral(Token::Token& result, Iterator cur)
{

}
