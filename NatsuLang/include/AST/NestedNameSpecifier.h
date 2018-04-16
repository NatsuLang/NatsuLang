#pragma once
#include <variant>
#include <natRefObj.h>

namespace NatsuLang
{
	namespace Identifier
	{
		class IdentifierInfo;
		using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
	}

	namespace Declaration
	{
		class Decl;
		class DeclContext;
		class ModuleDecl;
		class TagDecl;
		using DeclPtr = NatsuLib::natRefPointer<Decl>;
	}

	namespace Type
	{
		class Type;
		using TypePtr = NatsuLib::natRefPointer<Type>;
	}

	class ASTContext;

	class NestedNameSpecifier
		: public NatsuLib::natRefObjImpl<NatsuLib::natRefObj>
	{
	public:
		enum class SpecifierType
		{
			Module,
			Tag,
			Global,
			Outer,
		};

		struct Hash
		{
			std::size_t operator()(NatsuLib::natRefPointer<NestedNameSpecifier> const& nns) const noexcept
			{
				return std::hash<NatsuLib::natRefPointer<NestedNameSpecifier>>{}(nns->m_Prefix) ^ std::hash<Declaration::DeclPtr>
					{}(nns->m_Specifier) ^ std::hash<SpecifierType>{}(nns->m_SpecifierType);
			}
		};

		struct EqualTo
		{
			nBool operator()(NatsuLib::natRefPointer<NestedNameSpecifier> const& left,
			                 NatsuLib::natRefPointer<NestedNameSpecifier> const& right) const noexcept
			{
				return left->m_Prefix == right->m_Prefix && left->m_Specifier == right->m_Specifier && left->m_SpecifierType ==
					right->m_SpecifierType;
			}
		};

		NestedNameSpecifier();
		~NestedNameSpecifier();

		NatsuLib::natRefPointer<NestedNameSpecifier> GetPrefix() const noexcept;
		SpecifierType GetType() const noexcept;

		NatsuLib::natRefPointer<Declaration::ModuleDecl> GetAsModule() const noexcept;
		NatsuLib::natRefPointer<Declaration::TagDecl> GetAsTag() const noexcept;

		Declaration::DeclContext* GetAsDeclContext(ASTContext const& context) const noexcept;

		static NatsuLib::natRefPointer<NestedNameSpecifier> Create(ASTContext const& context,
		                                                           NatsuLib::natRefPointer<NestedNameSpecifier> prefix,
		                                                           Declaration::DeclPtr decl);
		static NatsuLib::natRefPointer<NestedNameSpecifier> Create(ASTContext const& context);
		static NatsuLib::natRefPointer<NestedNameSpecifier> Create(ASTContext const& context,
		                                                           NatsuLib::natRefPointer<NestedNameSpecifier> prefix);

	private:
		// 上一级嵌套名称
		NatsuLib::natRefPointer<NestedNameSpecifier> m_Prefix;
		// 可能是IdPtr、DeclPtr、TypePtr或者为空，为空时表示全局
		Declaration::DeclPtr m_Specifier;
		SpecifierType m_SpecifierType;

		static NatsuLib::natRefPointer<NestedNameSpecifier> FindOrInsert(ASTContext const& context,
		                                                                 NatsuLib::natRefPointer<NestedNameSpecifier> nns);
	};
}
