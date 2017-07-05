#include "Preprocessor.h"

using namespace NatsuLang;

Preprocessor::Preprocessor()
{
}

Preprocessor::~Preprocessor()
{
}

NatsuLib::natRefPointer<Identifier::IdentifierInfo> Preprocessor::FindIdentifierInfo(nStrView identifierName, Token::Token& token) const
{
	auto info = m_Table.GetOrAdd(identifierName);
	token.SetIdentifierInfo(info);
	token.SetType(info->GetTokenType());
	return info;
}
