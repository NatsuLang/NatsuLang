#include "Basic/Identifier.h"
#include "Basic/Token.h"

using namespace NatsuLib;
using namespace NatsuLang::Identifier;
using namespace NatsuLang::Token;

namespace
{
	constexpr bool IsKeyword(NatsuLang::Token::TokenType token) noexcept
	{
		switch (token)
		{
#define KEYWORD(X) case NatsuLang::Token::TokenType::Kw_ ## X:
#include "Basic/TokenDef.h"
			return true;
		default:
			return false;
		}
	}
}

IdentifierInfo::IdentifierInfo(nStrView name, Token::TokenType tokenType) noexcept
	: m_Name{ name }, m_TokenType{ tokenType }
{
}

nStrView IdentifierInfo::GetName() const noexcept
{
	return m_Name;
}

nBool IdentifierInfo::IsKeyword() const noexcept
{
	return ::IsKeyword(m_TokenType);
}

NatsuLang::Token::TokenType IdentifierInfo::GetTokenType() const noexcept
{
	return m_TokenType;
}

NatsuLang::Token::TokenType IdentifierInfo::SetTokenType(Token::TokenType tokenType) noexcept
{
	return std::exchange(m_TokenType, tokenType);
}

nInt IdentifierInfo::CompareTo(natRefPointer<IdentifierInfo> const& other) const
{
	return m_Name.Compare(other->m_Name);
}

natRefPointer<IdentifierInfo> IdentifierTable::GetOrAdd(nStrView name, Token::TokenType tokenType)
{
	auto iter = m_Identifiers.find(name);
	if (iter != m_Identifiers.end())
	{
		return iter->second;
	}

	nBool succeed;
	tie(iter, succeed) = m_Identifiers.emplace(name, make_ref<IdentifierInfo>(name, tokenType));
	if (!succeed)
	{
		nat_Throw(natErrException, NatErr_InternalErr, "Cannot add IdentifierInfo.");
	}

	return iter->second;
}

std::unordered_map<nString, natRefPointer<IdentifierInfo>>::const_iterator IdentifierTable::begin() const
{
	return m_Identifiers.begin();
}

std::unordered_map<nString, natRefPointer<IdentifierInfo>>::const_iterator IdentifierTable::end() const
{
	return m_Identifiers.end();
}

size_t IdentifierTable::size() const
{
	return m_Identifiers.size();
}
