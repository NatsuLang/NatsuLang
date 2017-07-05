#pragma once
#include <variant>
#include <optional>
#include <natRefObj.h>

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
		TokenNumber,
	};

	constexpr bool IsLiteral(TokenType tokenType) noexcept
	{
		return tokenType == TokenType::NumericLiteral || tokenType == TokenType::CharLiteral || tokenType == TokenType::StringLiteral;
	}

	const char* GetTokenName(TokenType tokenType) noexcept;
	const char* GetPunctuatorName(TokenType tokenType) noexcept;
	const char* GetKeywordName(TokenType tokenType) noexcept;

	class Token final
	{
	public:
		constexpr explicit Token(TokenType tokenType = TokenType::Unknown) noexcept
			: m_Type{ tokenType }
		{
		}

		void Reset() noexcept
		{
			m_Type = TokenType::Unknown;
			m_Info.reset();
		}

		TokenType GetType() const noexcept
		{
			return m_Type;
		}

		void SetType(TokenType tokenType) noexcept
		{
			m_Type = tokenType;
		}

		NatsuLib::natRefPointer<Identifier::IdentifierInfo> GetIdentifierInfo() noexcept
		{
			if (!m_Info.has_value())
			{
				return nullptr;
			}

			auto& info = m_Info.value();
			return info.index() == 0 ? std::get<0>(info) : nullptr;
		}

		void SetIdentifierInfo(NatsuLib::natRefPointer<Identifier::IdentifierInfo> identifierInfo) noexcept
		{
			m_Info.emplace(std::in_place_index<0>, std::move(identifierInfo));
		}

		std::optional<NatsuLib::nStrView> GetLiteralContent() noexcept
		{
			if (!m_Info.has_value())
			{
				return {};
			}

			auto& info = m_Info.value();
			if (info.index() == 1)
			{
				return std::get<1>(info);
			}

			return {};
		}

		void SetLiteralContent(NatsuLib::nStrView const& str) noexcept
		{
			m_Info.emplace(std::in_place_index<1>, str);
		}

	private:
		TokenType m_Type;
		std::optional<std::variant<NatsuLib::natRefPointer<NatsuLang::Identifier::IdentifierInfo>, NatsuLib::nStrView>> m_Info;
	};
}
