#include "Basic/Identifier.h"
#include "Basic/Token.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Identifier;
using namespace NatsuLang::Lex;

namespace
{
	constexpr bool IsKeyword(NatsuLang::Lex::TokenType token) noexcept
	{
		switch (token)
		{
#define KEYWORD(X) case NatsuLang::Lex::TokenType::Kw_ ## X:
#include "Basic/TokenDef.h"
			return true;
		default:
			return false;
		}
	}
}

IdentifierInfo::IdentifierInfo(nStrView name, Lex::TokenType tokenType) noexcept
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

NatsuLang::Lex::TokenType IdentifierInfo::GetTokenType() const noexcept
{
	return m_TokenType;
}

NatsuLang::Lex::TokenType IdentifierInfo::SetTokenType(Lex::TokenType tokenType) noexcept
{
	return std::exchange(m_TokenType, tokenType);
}

nInt IdentifierInfo::CompareTo(natRefPointer<IdentifierInfo> const& other) const
{
	return m_Name.GetView().Compare(other->m_Name);
}

natRefPointer<IdentifierInfo> IdentifierTable::GetOrAdd(nStrView name, Lex::TokenType tokenType)
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
