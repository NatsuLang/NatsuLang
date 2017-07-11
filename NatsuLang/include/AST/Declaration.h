#pragma once
#include "DeclBase.h"

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
}

namespace NatsuLang::Statement
{
	class LabelStmt;
}

namespace NatsuLang::Declaration
{
	class NamedDecl
		: public Decl
	{
	public:
		using IdPtr = NatsuLib::natRefPointer<Identifier::IdentifierInfo>;

		NamedDecl(Type type, DeclContext* context, SourceLocation loc, IdPtr identifierInfo)
			: Decl(type, context, loc), m_IdentifierInfo{ std::move(identifierInfo) }
		{
		}
		~NamedDecl();

		IdPtr GetIdentifierInfo() const noexcept
		{
			return m_IdentifierInfo;
		}

		void SetIdentifierInfo(IdPtr identifierInfo) noexcept
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
		IdPtr m_IdentifierInfo;
	};

	class LabelDecl
		: public NamedDecl
	{
	public:
		using LabelStmtPtr = NatsuLib::natWeakRefPointer<Statement::LabelStmt>;

		LabelDecl(DeclContext* context, SourceLocation loc, IdPtr identifierInfo, LabelStmtPtr stmt)
			: NamedDecl{ Type::Label, context, loc, identifierInfo }, m_Stmt{ std::move(stmt) }
		{
		}

		~LabelDecl()
		{
		}

		LabelStmtPtr GetStmt() const noexcept
		{
			return m_Stmt;
		}

		void SetStmt(LabelStmtPtr stmt) noexcept
		{
			m_Stmt = std::move(stmt);
		}

	private:
		LabelStmtPtr m_Stmt;
	};

}
