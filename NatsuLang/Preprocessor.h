#pragma once
#include "Identifier.h"
#include "Diagnostic.h"
#include "SourceManager.h"

namespace NatsuLang
{
	class Preprocessor
	{
	public:
		Preprocessor(Diag::DiagnosticsEngine& diag, SourceManager& sourceManager);
		~Preprocessor();

		NatsuLib::natRefPointer<Identifier::IdentifierInfo> FindIdentifierInfo(nStrView identifierName, Token::Token& token) const;

		Diag::DiagnosticsEngine& GetDiag() const noexcept
		{
			return m_Diag;
		}

	private:
		mutable Identifier::IdentifierTable m_Table;
		Diag::DiagnosticsEngine& m_Diag;
		SourceManager& m_SourceManager;
	};
}
