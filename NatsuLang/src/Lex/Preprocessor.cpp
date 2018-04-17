#include "Lex/Preprocessor.h"

using namespace NatsuLib;
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
	m_CachedTokensStack.emplace_back(move(tokens), std::vector<Lex::Token>::const_iterator{});
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
#define KEYWORD(X) m_Table.GetOrAdd(#X ## _nv, Lex::TokenType::Kw_ ## X);
#include "Basic/TokenDef.h"
}

Preprocessor::Memento Preprocessor::SaveToMemento() noexcept
{
	if (m_CachedTokensStack.empty())
	{
		assert(m_Lexer);
		return { std::in_place_index<1>, m_Lexer->SaveToMemento() };
	}
	
	const auto curIter = std::prev(m_CachedTokensStack.end());
	return { std::in_place_index<0>, curIter, curIter->second };
}

void Preprocessor::RestoreFromMemento(Memento const& memento) noexcept
{
	if (memento.m_Content.index() == 0)
	{
		auto const& pair = std::get<0>(memento.m_Content);
		m_CachedTokensStack.erase(std::next(pair.first), m_CachedTokensStack.end());
		pair.first->second = pair.second;
	}
	else
	{
		assert(m_Lexer);
		assert(memento.m_Content.index() == 1);
		m_Lexer->RestoreFromMemento(std::get<1>(memento.m_Content));
	}
}
