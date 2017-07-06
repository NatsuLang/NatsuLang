#include "Preprocessor.h"

using namespace NatsuLang;

Preprocessor::Preprocessor(Diag::DiagnosticsEngine& diag, SourceManager& sourceManager)
	: m_Diag{ diag }, m_SourceManager{ sourceManager }
{
}

Preprocessor::~Preprocessor()
{
}

NatsuLib::natRefPointer<Identifier::IdentifierInfo> Preprocessor::FindIdentifierInfo(nStrView identifierName, Token::Token& token) const
{
	auto info = m_Table.GetOrAdd(identifierName, NatsuLang::Token::TokenType::Identifier);
	token.SetIdentifierInfo(info);
	token.SetType(info->GetTokenType());
	return info;
}
