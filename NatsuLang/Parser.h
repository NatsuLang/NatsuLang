#pragma once
#include <unordered_set>
#include "Preprocessor.h"

namespace NatsuLang::Declaration
{
	class Decl;
}

namespace NatsuLang::Diag
{
	class DiagnosticsEngine;
}

namespace NatsuLang::Syntax
{
	class Parser
	{
	public:
		Parser(Preprocessor& preprocessor, Diag::DiagnosticsEngine& diag);
		~Parser();

		Preprocessor& GetPreprocessor() const noexcept;
		Diag::DiagnosticsEngine& GetDiagnosticsEngine() const noexcept;

		void ConsumeToken()
		{
			m_PrevTokenLocation = m_CurrentToken.GetLocation();
			m_Preprocessor.Lex(m_CurrentToken);
		}

		nBool ParseTopLevelDecl(std::unordered_set<NatsuLib::natRefPointer<Declaration::Decl>>& decls);

		nBool ParseModule();

	private:
		Preprocessor& m_Preprocessor;
		Diag::DiagnosticsEngine& m_DiagnosticsEngine;

		Token::Token m_CurrentToken;
		SourceLocation m_PrevTokenLocation;
	};
}
