#pragma once
#include "DeclBase.h"

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
}

namespace NatsuLang::Declaration
{
	class NamedDecl
		: public Decl
	{
	public:
		NamedDecl(Type type, DeclContext* context, SourceLocation loc, NatsuLib::natRefPointer<Identifier::IdentifierInfo> identifierInfo)
			: Decl(type, context, loc), m_IdentifierInfo{ std::move(identifierInfo) }
		{
		}
		~NamedDecl();

		NatsuLib::natRefPointer<Identifier::IdentifierInfo> GetIdentifierInfo() const noexcept
		{
			return m_IdentifierInfo;
		}

		void SetIdentifierInfo(NatsuLib::natRefPointer<Identifier::IdentifierInfo> identifierInfo) noexcept
		{
			m_IdentifierInfo = std::move(identifierInfo);
		}

		nStrView GetName() const noexcept;

		nBool HasLinkage() const noexcept
		{
			static_cast<void>(this);
			return true;
		}

	private:
		NatsuLib::natRefPointer<Identifier::IdentifierInfo> m_IdentifierInfo;
	};
}
