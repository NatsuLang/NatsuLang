﻿#pragma once
#include <unordered_set>
#include <natRefObj.h>
#include <natLinq.h>
#include "Basic/SourceLocation.h"
#include "Basic/Specifier.h"
#include "Basic/Token.h"

namespace NatsuLang
{
	struct ICompilerAction;
}

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

	struct IAttribute;
	using AttrPtr = NatsuLib::natRefPointer<IAttribute>;

	class ParmVarDecl;

	enum class Context
	{
		Global,
		Prototype,
		TypeName,
		Member,
		Block,
		For,
		Catch,
	};

	class Declarator final
		: public NatsuLib::natRefObjImpl<Declarator>
	{
	public:
		explicit Declarator(Context context);

		~Declarator();

		SourceLocation GetIdentifierLocation() const noexcept
		{
			return m_IdLocation;
		}

		void SetIdentifierLocation(SourceLocation value) noexcept
		{
			m_IdLocation = value;
		}

		Identifier::IdPtr GetIdentifier() const noexcept
		{
			return m_Identifier.index() == 0 ? std::get<0>(m_Identifier) : nullptr;
		}

		void SetIdentifier(Identifier::IdPtr idPtr) noexcept
		{
			m_Identifier = std::move(idPtr);
		}

		nBool IsConstructor() const noexcept
		{
			return m_Identifier.index() == 1;
		}

		nBool IsDestructor() const noexcept
		{
			return m_Identifier.index() == 2;
		}

		void SetConstructor() noexcept
		{
			m_Identifier.emplace<1>();
		}

		void SetDestructor() noexcept
		{
			m_Identifier.emplace<2>();
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

		Specifier::Safety GetSafety() const noexcept
		{
			return m_Safety;
		}

		void SetSafety(Specifier::Safety value) noexcept
		{
			m_Safety = value;
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

		void ClearParams() noexcept
		{
			m_Params.clear();
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

		DeclPtr GetDeclarationContext() const noexcept
		{
			return m_DeclarationContext;
		}

		void SetDeclarationContext(DeclPtr value) noexcept
		{
			m_DeclarationContext = std::move(value);
		}

		nBool IsValid() const noexcept
		{
			return (m_Identifier.index() != 0 || std::get<0>(m_Identifier)) || m_Type || m_Initializer;
		}

		nBool IsUnresolved() const noexcept
		{
			// 若为 Unresolved 则必须类型和初始化器也都为空
			return !m_CachedTokens.empty();
		}

		nBool IsAlias() const noexcept
		{
			return m_IsAlias;
		}

		void SetAlias(nBool value) noexcept
		{
			m_IsAlias = value;
		}

		void AttachPostProcessor(NatsuLib::natRefPointer<ICompilerAction> action);
		NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<ICompilerAction>>> GetPostProcessors() const noexcept;
		void ClearPostProcessors();

	private:
		struct ConstructorTag
		{
		};

		struct DestructorTag
		{
		};

		SourceRange m_Range;
		Context m_Context;
		Specifier::StorageClass m_StorageClass;
		Specifier::Access m_Accessibility;
		Specifier::Safety m_Safety;
		std::variant<Identifier::IdPtr, ConstructorTag, DestructorTag> m_Identifier;
		SourceLocation m_IdLocation;
		Type::TypePtr m_Type;
		Statement::StmtPtr m_Initializer;

		DeclPtr m_Decl;

		// 如果声明的是一个函数，这个 vector 将会保存参数信息
		std::vector<NatsuLib::natRefPointer<ParmVarDecl>> m_Params;

		// 保留缓存的 Token 以便延迟分析类型及初始化器或函数体等信息
		std::vector<Lex::Token> m_CachedTokens;

		NatsuLib::natRefPointer<Semantic::Scope> m_DeclarationScope;
		DeclPtr m_DeclarationContext;

		nBool m_IsAlias;

		std::unordered_set<NatsuLib::natRefPointer<ICompilerAction>> m_PostProcessors;
	};

	using DeclaratorPtr = NatsuLib::natRefPointer<Declarator>;
}
