#include "Lex/Preprocessor.h"

using namespace NatsuLang;

Preprocessor::Preprocessor(Diag::DiagnosticsEngine& diag, SourceManager& sourceManager)
	: m_Diag{ diag }, m_SourceManager{ sourceManager }, m_CurrentCachedToken{ m_CachedTokens.cend() }
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

void Preprocessor::SetCachedTokens(std::vector<Lex::Token> tokens)
{
	m_CachedTokens = move(tokens);
	m_CurrentCachedToken = m_CachedTokens.cbegin();
}

void Preprocessor::ClearCachedTokens()
{
	m_CachedTokens.clear();
	m_CurrentCachedToken = m_CachedTokens.cend();
}

nBool Preprocessor::Lex(Lex::Token& result)
{
	if (m_CurrentCachedToken != m_CachedTokens.cend())
	{
		result = *m_CurrentCachedToken++;
		return true;
	}

	return m_Lexer ? m_Lexer->Lex(result) : false;
}

void Preprocessor::init() const
{
#define KEYWORD(X) m_Table.GetOrAdd(#X, Lex::TokenType::Kw_ ## X);
#include "Basic/TokenDef.h"
}
