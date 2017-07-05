#pragma once
#include "Identifier.h"

namespace NatsuLang
{
	class Preprocessor
	{
	public:
		Preprocessor();
		~Preprocessor();

		NatsuLib::natRefPointer<Identifier::IdentifierInfo> FindIdentifierInfo(nStrView identifierName, Token::Token& token) const;

	private:
		mutable Identifier::IdentifierTable m_Table;
	};
}
