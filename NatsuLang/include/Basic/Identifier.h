#pragma once
#include "Token.h"
#include <natRefObj.h>
#include <natRelationalOperator.h>
#include <unordered_map>

namespace NatsuLang::Identifier
{
	class IdentifierInfo final
		: public NatsuLib::natRefObjImpl<IdentifierInfo, NatsuLib::RelationalOperator::IComparable<NatsuLib::natRefPointer<IdentifierInfo>>>
	{
	public:
		IdentifierInfo(nStrView name = {}, Token::TokenType tokenType = Token::TokenType::Identifier) noexcept;

		nStrView GetName() const noexcept;
		nBool IsKeyword() const noexcept;
		Token::TokenType GetTokenType() const noexcept;
		Token::TokenType SetTokenType(Token::TokenType tokenType) noexcept;

		nInt CompareTo(NatsuLib::natRefPointer<IdentifierInfo> const& other) const override;

	private:
		nStrView m_Name;
		Token::TokenType m_TokenType;
	};

	class IdentifierTable final
		: public NatsuLib::natRefObjImpl<IdentifierTable>
	{
	public:
		NatsuLib::natRefPointer<IdentifierInfo> GetOrAdd(nStrView name, Token::TokenType tokenType = Token::TokenType::Identifier);

		std::unordered_map<nString, NatsuLib::natRefPointer<IdentifierInfo>>::const_iterator begin() const;
		std::unordered_map<nString, NatsuLib::natRefPointer<IdentifierInfo>>::const_iterator end() const;
		std::size_t size() const;

	private:
		std::unordered_map<nString, NatsuLib::natRefPointer<IdentifierInfo>> m_Identifiers;
	};
}
