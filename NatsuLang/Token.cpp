#include "Token.h"

using namespace std;
using namespace NatsuLang::Token;

namespace
{
	constexpr const char* g_TokenNames[]
	{
#define TOK(X) #X,
#define KEYWORD(X,Y) #X,
#include "TokenDef.h"
	};
}

const char* NatsuLang::Token::GetTokenName(TokenType tokenType) noexcept
{
	const auto index = static_cast<size_t>(tokenType);
	if (index < static_cast<size_t>(TokenType::TokenNumber))
	{
		return g_TokenNames[index];
	}
	
	return nullptr;
}

const char* NatsuLang::Token::GetPunctuatorName(TokenType tokenType) noexcept
{
	switch (tokenType)
	{
#define PUNCTUATOR(X,Y) case TokenType::X: return Y;
#include "TokenDef.h"
	default:
		break;
	}

	return nullptr;
}

const char* NatsuLang::Token::GetKeywordName(TokenType tokenType) noexcept
{
	switch (tokenType)
	{
#define KEYWORD(X,Y) case TokenType::Kw_ ## X: return #X;
#include "TokenDef.h"
	default:
		break;
	}

	return nullptr;
}
