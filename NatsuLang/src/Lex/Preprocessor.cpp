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

void Preprocessor::init() const
{
#define KEYWORD(X) m_Table.GetOrAdd(#X, Lex::TokenType::Kw_ ## X);
#include "Basic/TokenDef.h"
}
