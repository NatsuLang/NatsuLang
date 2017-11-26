#include "Lex/Preprocessor.h"

using namespace NatsuLang;

Preprocessor::Preprocessor(Diag::DiagnosticsEngine& diag, SourceManager& sourceManager)
	: m_Diag{ diag }, m_SourceManager{ sourceManager }
{
	init();
}

Preprocessor::~Preprocessor()
{
}

NatsuLib::natRefPointer<Identifier::IdentifierInfo> Preprocessor::FindIdentifierInfo(nStrView identifierName, Lex::Token& token) const
{
	auto info = m_Table.GetOrAdd(identifierName, Lex::TokenType::Identifier);
	token.SetIdentifierInfo(info);
	token.SetType(info->GetTokenType());
	return info;
}

void Preprocessor::PushCachedTokens(std::vector<Lex::Token> tokens)
{
	m_CachedTokensStack.emplace_back(std::pair<std::vector<Lex::Token>, std::vector<Lex::Token>::const_iterator>{ move(tokens), {} });
	auto& cachedTokensPair = m_CachedTokensStack.back();
	cachedTokensPair.second = cachedTokensPair.first.cbegin();
}

void Preprocessor::PopCachedTokens()
{
	m_CachedTokensStack.pop_back();
}

nBool Preprocessor::Lex(Lex::Token& result)
{
	if (!m_CachedTokensStack.empty())
	{
		auto& cachedTokensPair = m_CachedTokensStack.back();
		if (cachedTokensPair.second != cachedTokensPair.first.cend())
		{
			result = *cachedTokensPair.second++;
			return true;
		}

		return false;
	}

	return m_Lexer ? m_Lexer->Lex(result) : false;
}

void Preprocessor::init() const
{
#define KEYWORD(X) m_Table.GetOrAdd(#X, Lex::TokenType::Kw_ ## X);
#include "Basic/TokenDef.h"
}
