#pragma once
#include <natRefObj.h>
#include <natLinq.h>
#include "Basic/SourceLocation.h"
#include "Basic/Specifier.h"

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

namespace NatsuLang::Semantic
{
	class Scope;
}

namespace NatsuLang::Declaration
{
	class Decl;
	using DeclPtr = NatsuLib::natRefPointer<Decl>;

	class ParmVarDecl;

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

	class Declarator final
		: public NatsuLib::natRefObjImpl<Declarator>
	{
	public:
		explicit Declarator(Context context)
			: m_Context{ context }, m_StorageClass{ Specifier::StorageClass::None }, m_Accessibility{ Specifier::Access::None }
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

		Specifier::StorageClass GetStorageClass() const noexcept
		{
			return m_StorageClass;
		}

		void SetStorageClass(Specifier::StorageClass value) noexcept
		{
			m_StorageClass = value;
		}

		Specifier::Access GetAccessibility() const noexcept
		{
			return m_Accessibility;
		}

		void SetAccessibility(Specifier::Access value) noexcept
		{
			m_Accessibility = value;
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

		DeclPtr GetDecl() const noexcept
		{
			return m_Decl;
		}

		void SetDecl(DeclPtr value) noexcept
		{
			m_Decl = std::move(value);
		}

		std::vector<NatsuLib::natRefPointer<ParmVarDecl>> const& GetParams() const noexcept
		{
			return m_Params;
		}

		void SetParams(NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<ParmVarDecl>>> params) noexcept
		{
			m_Params.assign(params.begin(), params.end());
		}

		std::vector<Lex::Token> const& GetCachedTokens() const noexcept
		{
			return m_CachedTokens;
		}

		std::vector<Lex::Token> GetAndClearCachedTokens() noexcept
		{
			return move(m_CachedTokens);
		}

		void ClearCachedTokens() noexcept
		{
			m_CachedTokens.clear();
		}

		void SetCachedTokens(std::vector<Lex::Token> value) noexcept
		{
			m_CachedTokens = move(value);
		}

		NatsuLib::natRefPointer<Semantic::Scope> GetDeclarationScope() const noexcept
		{
			return m_DeclarationScope;
		}

		void SetDeclarationScope(NatsuLib::natRefPointer<Semantic::Scope> value) noexcept
		{
			m_DeclarationScope = std::move(value);
		}

		nBool IsValid() const noexcept
		{
			return m_Identifier || m_Type || m_Initializer;
		}

		nBool IsUnresolved() const noexcept
		{
			// 若为 Unresolved 则必须类型和初始化器也都为空
			return !m_CachedTokens.empty();
		}

	private:
		SourceRange m_Range;
		Context m_Context;
		Specifier::StorageClass m_StorageClass;
		Specifier::Access m_Accessibility;
		Identifier::IdPtr m_Identifier;
		Type::TypePtr m_Type;
		Statement::StmtPtr m_Initializer;

		DeclPtr m_Decl;

		// 如果声明的是一个函数，这个 vector 将会保存参数信息
		std::vector<NatsuLib::natRefPointer<ParmVarDecl>> m_Params;

		// 保留缓存的 Token 以便延迟分析类型及初始化器或函数体等信息
		std::vector<Lex::Token> m_CachedTokens;

		NatsuLib::natRefPointer<Semantic::Scope> m_DeclarationScope;
	};

	using DeclaratorPtr = NatsuLib::natRefPointer<Declarator>;
}
