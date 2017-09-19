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

Identifier::IdPtr NestedNameSpecifier::GetAsIdentifier() const noexcept
{
	return m_Specifier;
}

natRefPointer<Declaration::ModuleDecl> NestedNameSpecifier::GetAsModule() const noexcept
{
	return m_Specifier;
}

Type::TypePtr NestedNameSpecifier::GetAsType() const noexcept
{
	return m_Specifier;
}

Declaration::DeclContext* NestedNameSpecifier::GetAsDeclContext(ASTContext const& context) const noexcept
{
	switch (m_SpecifierType)
	{
	case SpecifierType::Module:
		return GetAsModule().Get();
	case SpecifierType::Type:
	{
		auto ret = static_cast<natRefPointer<Type::TagType>>(GetAsType());
		assert(ret);
		return ret->GetDecl().Get();
	}
	case SpecifierType::Global:
		return context.GetTranslationUnit().Get();
	case SpecifierType::Outer:
		// TODO: 完成获得外层名称的DeclContext
	case SpecifierType::Identifier:
	default: 
		return nullptr;
	}
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context, natRefPointer<NestedNameSpecifier> prefix, Identifier::IdPtr id)
{
	auto ret = make_ref<NestedNameSpecifier>();
	ret->m_Prefix = std::move(prefix);
	ret->m_Specifier = id;
	ret->m_SpecifierType = SpecifierType::Identifier;

	return FindOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context, natRefPointer<NestedNameSpecifier> prefix, natRefPointer<Declaration::ModuleDecl> module)
{
	auto ret = make_ref<NestedNameSpecifier>();
	ret->m_Prefix = std::move(prefix);
	ret->m_Specifier = module;
	ret->m_SpecifierType = SpecifierType::Module;

	return FindOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context, natRefPointer<NestedNameSpecifier> prefix, Type::TypePtr type)
{
	auto ret = make_ref<NestedNameSpecifier>();
	ret->m_Prefix = std::move(prefix);
	ret->m_Specifier = type;
	ret->m_SpecifierType = SpecifierType::Type;

	return FindOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context)
{
	auto ret = make_ref<NestedNameSpecifier>();
	return FindOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::Create(ASTContext const& context, natRefPointer<NestedNameSpecifier> prefix)
{
	auto ret = make_ref<NestedNameSpecifier>();
	ret->m_Prefix = std::move(prefix);
	ret->m_SpecifierType = SpecifierType::Outer;

	return FindOrInsert(context, std::move(ret));
}

natRefPointer<NestedNameSpecifier> NestedNameSpecifier::FindOrInsert(ASTContext const& context, natRefPointer<NestedNameSpecifier> nns)
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
