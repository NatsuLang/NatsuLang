#pragma once
#include <natRefObj.h>
#include "Basic/SourceLocation.h"

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
	using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
}

namespace NatsuLang::Type
{
	class Type;
	using TypePtr = NatsuLib::natRefPointer<Type>;
}

namespace NatsuLang::Statement
{
	class Stmt;
	using StmtPtr = NatsuLib::natRefPointer<Stmt>;
}

namespace NatsuLang::Expression
{
	class Expr;
	using ExprPtr = NatsuLib::natRefPointer<Expr>;
}

namespace NatsuLang::Declaration
{
	class Decl;
	using DeclPtr = NatsuLib::natRefPointer<Decl>;

	enum class Context
	{
		Global,
		Prototype,
		TypeName,
		Member,
		Block,
		For,
		New,
		Catch,
	};

	class Declarator
	{
	public:
		explicit Declarator(Context context)
			: m_Context{ context }, m_TypeSpecifier{}
		{
		}

		Identifier::IdPtr GetIdentifier() const noexcept
		{
			return m_Identifier;
		}

		void SetIdentifier(Identifier::IdPtr idPtr) noexcept
		{
			m_Identifier = std::move(idPtr);
		}

		SourceRange GetRange() const noexcept
		{
			return m_Range;
		}

		void SetRange(SourceRange range) noexcept
		{
			m_Range = range;
		}

		Context GetContext() const noexcept
		{
			return m_Context;
		}

		void SetContext(Context context) noexcept
		{
			m_Context = context;
		}

		Specifier::TypeSpecifier GetTypeSpecifier() const noexcept
		{
			return m_TypeSpecifier;
		}

		void SetTypeSpecifier(Specifier::TypeSpecifier value) noexcept
		{
			m_TypeSpecifier = value;
		}

		Type::TypePtr GetType() const noexcept
		{
			return m_Type;
		}

		void SetType(Type::TypePtr value) noexcept
		{
			m_Type = std::move(value);
		}

		Statement::StmtPtr GetInitializer() const noexcept
		{
			return m_Initializer;
		}

		void SetInitializer(Statement::StmtPtr value) noexcept
		{
			m_Initializer = std::move(value);
		}

		nBool IsValid() const noexcept
		{
			return m_Identifier || m_Type || m_Initializer;
		}

	private:
		SourceRange m_Range;
		Context m_Context;
		Identifier::IdPtr m_Identifier;
		Specifier::TypeSpecifier m_TypeSpecifier;
		Type::TypePtr m_Type;
		Statement::StmtPtr m_Initializer;
	};
}
