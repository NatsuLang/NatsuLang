#pragma once
#include <unordered_set>
#include "Lex/Preprocessor.h"

namespace NatsuLang::Declaration
{
	class Decl;
}

namespace NatsuLang::Diag
{
	class DiagnosticsEngine;
}

namespace NatsuLang::Semantic
{
	class Sema;
}

namespace NatsuLang::Syntax
{
	class Parser
	{
	public:
		Parser(Preprocessor& preprocessor, Semantic::Sema& sema);
		~Parser();

		Preprocessor& GetPreprocessor() const noexcept;
		Diag::DiagnosticsEngine& GetDiagnosticsEngine() const noexcept;

		void ConsumeToken()
		{
			m_PrevTokenLocation = m_CurrentToken.GetLocation();
			m_Preprocessor.Lex(m_CurrentToken);
		}

		void ConsumeParen()
		{
			assert(IsParen(m_CurrentToken.GetType()));
			if (m_CurrentToken.Is(Token::TokenType::LeftParen))
			{
				++m_ParenCount;
			}
			else if (m_ParenCount)
			{
				--m_ParenCount;
			}
			ConsumeToken();
		}

		void ConsumeBracket()
		{
			assert(IsBracket(m_CurrentToken.GetType()));
			if (m_CurrentToken.Is(Token::TokenType::LeftSquare))
			{
				++m_BracketCount;
			}
			else if (m_BracketCount)
			{
				--m_BracketCount;
			}
			ConsumeToken();
		}

		void ConsumeBrace()
		{
			assert(IsBrace(m_CurrentToken.GetType()));
			if (m_CurrentToken.Is(Token::TokenType::LeftBrace))
			{
				++m_BraceCount;
			}
			else if (m_BraceCount)
			{
				--m_BraceCount;
			}
			ConsumeToken();
		}

		void ConsumeAnyToken()
		{
			const auto type = m_CurrentToken.GetType();
			if (IsParen(type))
			{
				ConsumeParen();
			}
			else if (IsBracket(type))
			{
				ConsumeBracket();
			}
			else if (IsBrace(type))
			{
				ConsumeBrace();
			}
			else
			{
				ConsumeToken();
			}
		}

		nBool ParseTopLevelDecl(std::unordered_set<NatsuLib::natRefPointer<Declaration::Decl>>& decls);

		std::vector<NatsuLib::natRefPointer<Declaration::Decl>> ParseModuleImport();
		nBool ParseModuleName(std::vector<std::pair<NatsuLib::natRefPointer<Identifier::IdentifierInfo>, SourceLocation>>& path);

		nBool SkipUntil(std::initializer_list<Token::TokenType> list, nBool dontConsume = false);

	private:
		Preprocessor& m_Preprocessor;
		Diag::DiagnosticsEngine& m_DiagnosticsEngine;
		Semantic::Sema& m_Sema;

		Token::Token m_CurrentToken;
		SourceLocation m_PrevTokenLocation;
		nuInt m_ParenCount, m_BracketCount, m_BraceCount;
	};
}
