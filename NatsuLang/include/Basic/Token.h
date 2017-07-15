#pragma once
#include <variant>
#include <optional>
#include <natRefObj.h>
#include "SourceLocation.h"

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
}

namespace NatsuLang::Token
{
	enum class TokenType
	{
#define TOK(X) X,
#include "TokenDef.h"
		TokenCount,
	};

	constexpr bool IsLiteral(TokenType tokenType) noexcept
	{
		return tokenType == TokenType::NumericLiteral || tokenType == TokenType::CharLiteral || tokenType == TokenType::StringLiteral;
	}

	constexpr bool IsParen(TokenType tokenType) noexcept
	{
		return tokenType == TokenType::LeftParen || tokenType == TokenType::RightParen;
	}

	constexpr bool IsBracket(TokenType tokenType) noexcept
	{
		return tokenType == TokenType::LeftSquare || tokenType == TokenType::RightSquare;
	}

	constexpr bool IsBrace(TokenType tokenType) noexcept
	{
		return tokenType == TokenType::LeftBrace || tokenType == TokenType::RightBrace;
	}

	const char* GetTokenName(TokenType tokenType) noexcept;
	const char* GetPunctuatorName(TokenType tokenType) noexcept;
	const char* GetKeywordName(TokenType tokenType) noexcept;

	class Token final
	{
	public:
		constexpr explicit Token(TokenType tokenType = TokenType::Unknown, SourceLocation location = {}) noexcept
			: m_Type{ tokenType }, m_Location{ location }
		{
		}

		void Reset() noexcept
		{
			m_Type = TokenType::Unknown;
		}

		TokenType GetType() const noexcept
		{
			return m_Type;
		}

		void SetType(TokenType tokenType) noexcept
		{
			m_Type = tokenType;
		}

		nBool Is(TokenType tokenType) const noexcept
		{
			return m_Type == tokenType;
		}

		nBool IsAnyOf(std::initializer_list<TokenType> list) const noexcept
		{
			for (auto item : list)
			{
				if (m_Type == item)
				{
					return true;
				}
			}

			return false;
		}

		nuInt GetLength() const noexcept;

		void SetLength(nuInt value)
		{
			m_Info.emplace<2>(value);
		}

		SourceLocation GetLocation() const noexcept
		{
			return m_Location;
		}

		void SetLocation(SourceLocation location) noexcept
		{
			m_Location = location;
		}

		NatsuLib::natRefPointer<Identifier::IdentifierInfo> GetIdentifierInfo() noexcept
		{
			return m_Info.index() == 0 ? std::get<0>(m_Info) : nullptr;
		}

		void SetIdentifierInfo(NatsuLib::natRefPointer<Identifier::IdentifierInfo> identifierInfo)
		{
			m_Info.emplace<0>(std::move(identifierInfo));
		}

		std::optional<nStrView> GetLiteralContent() noexcept
		{
			if (m_Info.index() == 1)
			{
				return std::get<1>(m_Info);
			}

			return {};
		}

		void SetLiteralContent(nStrView const& str)
		{
			m_Info.emplace<1>(str);
		}

	private:
		TokenType m_Type;
		std::variant<NatsuLib::natRefPointer<Identifier::IdentifierInfo>, nStrView, nuInt> m_Info;
		SourceLocation m_Location;
	};
}
