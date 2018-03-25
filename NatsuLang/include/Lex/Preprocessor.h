#pragma once
#include "Basic/Identifier.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "Lexer.h"
#include <list>

namespace NatsuLang
{
	class Preprocessor
	{
	public:
		Preprocessor(Diag::DiagnosticsEngine& diag, SourceManager& sourceManager);
		~Preprocessor();

		NatsuLib::natRefPointer<Identifier::IdentifierInfo> FindIdentifierInfo(nStrView identifierName, Lex::Token& token) const;

		Diag::DiagnosticsEngine& GetDiag() const noexcept
		{
			return m_Diag;
		}

		SourceManager& GetSourceManager() const noexcept
		{
			return m_SourceManager;
		}

		void PushCachedTokens(std::vector<Lex::Token> tokens);
		void PopCachedTokens();

		nBool IsUsingCache() const noexcept
		{
			return !m_CachedTokensStack.empty();
		}

		NatsuLib::natRefPointer<Lex::Lexer> GetLexer() const noexcept
		{
			return m_Lexer;
		}

		void SetLexer(NatsuLib::natRefPointer<Lex::Lexer> lexer) noexcept
		{
			m_Lexer = std::move(lexer);
		}

		nBool Lex(Lex::Token& result);

	private:
		mutable Identifier::IdentifierTable m_Table;
		Diag::DiagnosticsEngine& m_Diag;
		SourceManager& m_SourceManager;
		NatsuLib::natRefPointer<Lex::Lexer> m_Lexer;

		using CachedTokensStackType = std::list<std::pair<std::vector<Lex::Token>, std::vector<Lex::Token>::const_iterator>>;
		CachedTokensStackType m_CachedTokensStack;

		void init() const;

		class Memento
		{
			friend class Preprocessor;
			using ContentType = std::variant<std::pair<CachedTokensStackType::iterator, std::vector<Lex::Token>::const_iterator>, Lex::Lexer::Memento>;

			template <typename... Args>
			constexpr Memento(Args&&... args)
				: m_Content(std::forward<Args>(args)...)
			{
			}

			ContentType m_Content;
		};

	public:
		Memento SaveToMemento() noexcept;
		void RestoreFromMemento(Memento const& memento) noexcept;
	};
}
