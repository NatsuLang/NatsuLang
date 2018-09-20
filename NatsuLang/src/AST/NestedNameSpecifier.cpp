#include "AST/NestedNameSpecifier.h"
#include "AST/ASTContext.h"
#include "AST/Expression.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

NestedNameSpecifier::NestedNameSpecifier()
	: m_SpecifierType{ SpecifierType::Global }
{
}

NestedNameSpecifier::~NestedNameSpecifier()
{
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::GetPrefix() const noexcept
{
	return m_Prefix;
}

NestedNameSpecifier::SpecifierType NestedNameSpecifier::GetType() const noexcept
{
	return m_SpecifierType;
}

natRefPointer<Declaration::ModuleDecl> NestedNameSpecifier::GetAsModule() const noexcept
{
	return m_Specifier;
}

natRefPointer<Declaration::TagDecl> NestedNameSpecifier::GetAsTag() const noexcept
{
	return m_Specifier;
}

Declaration::DeclContext* NestedNameSpecifier::GetAsDeclContext(ASTContext const& context) const noexcept
{
	switch (m_SpecifierType)
	{
	case SpecifierType::Module:
		return GetAsModule().Get();
	case SpecifierType::Tag:
		return GetAsTag().Get();
	case SpecifierType::Global:
		return context.GetTranslationUnit().Get();
	case SpecifierType::Outer:
		// TODO: 完成获得外层名称的DeclContext
	default: 
		return nullptr;
	}
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context, natRefPointer<NestedNameSpecifier> prefix, Declaration::DeclPtr decl)
{
	auto ret = make_ref<NestedNameSpecifier>();
	ret->m_Prefix = std::move(prefix);

	if (decl.Cast<Declaration::ModuleDecl>())
	{
		ret->m_SpecifierType = SpecifierType::Module;
	}
	else if (decl.Cast<Declaration::TagDecl>())
	{
		ret->m_SpecifierType = SpecifierType::Tag;
	}
	else
	{
		return nullptr;
	}

	ret->m_Specifier = std::move(decl);

	return findOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context)
{
	auto ret = make_ref<NestedNameSpecifier>();
	return findOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context, natRefPointer<NestedNameSpecifier> prefix)
{
	auto ret = make_ref<NestedNameSpecifier>();
	ret->m_Prefix = std::move(prefix);
	ret->m_SpecifierType = SpecifierType::Outer;

	return findOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::findOrInsert(ASTContext const& context, natRefPointer<NestedNameSpecifier> nns)
{
	const auto iter = context.m_NestedNameSpecifiers.find(nns);
	if (iter != context.m_NestedNameSpecifiers.end())
	{
		return *iter;
	}

	const auto result = context.m_NestedNameSpecifiers.emplace(std::move(nns));
	if (!result.second)
	{
		nat_Throw(natErrException, NatErr_InternalErr, "Cannot insert NestedNameSpecifier to context.");
	}

	return *result.first;
}
