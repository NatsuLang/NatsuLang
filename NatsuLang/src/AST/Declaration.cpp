#include "AST/Declaration.h"
#include "AST/ASTContext.h"
#include "AST/Expression.h"
#include "AST/DeclVisitor.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Declaration;

TranslationUnitDecl::TranslationUnitDecl(ASTContext& context)
	: Decl{ TranslationUnit }, DeclContext{ TranslationUnit }, m_Context{ context }
{
}

TranslationUnitDecl::~TranslationUnitDecl()
{
}

NatsuLang::ASTContext& TranslationUnitDecl::GetASTContext() const noexcept
{
	return m_Context;
}

NamedDecl::~NamedDecl()
{
}

nStrView NamedDecl::GetName() const noexcept
{
	return m_IdentifierInfo->GetName();
}

ModuleDecl::~ModuleDecl()
{
}

ValueDecl::~ValueDecl()
{
}

DeclaratorDecl::~DeclaratorDecl()
{
}

VarDecl::~VarDecl()
{
}

ImplicitParamDecl::~ImplicitParamDecl()
{
}

ParmVarDecl::~ParmVarDecl()
{
}

FunctionDecl::~FunctionDecl()
{
}

Linq<Valued<natRefPointer<ParmVarDecl>>> FunctionDecl::GetParams() const noexcept
{
	return from(m_Params);
}

void FunctionDecl::SetParams(Linq<Valued<natRefPointer<ParmVarDecl>>> value) noexcept
{
	m_Params.assign(value.begin(), value.end());
}

MethodDecl::~MethodDecl()
{
}

FieldDecl::~FieldDecl()
{
}

EnumConstantDecl::~EnumConstantDecl()
{
}

TypeDecl::~TypeDecl()
{
}

TagDecl::~TagDecl()
{
}

EnumDecl::~EnumDecl()
{
}

Linq<Valued<natRefPointer<EnumConstantDecl>>> EnumDecl::GetEnumerators() const noexcept
{
	return from(GetDecls())
		.where([](natRefPointer<Decl> const& decl)
		{
			return decl->GetType() == EnumConstant;
		})
		.select([](natRefPointer<Decl> const& decl)
		{
			return static_cast<natRefPointer<EnumConstantDecl>>(decl);
		});
}

ClassDecl::~ClassDecl()
{
}

Linq<Valued<natRefPointer<FieldDecl>>> ClassDecl::GetFields() const noexcept
{
	return from(GetDecls())
		.select([](natRefPointer<Decl> const& decl)
		{
			return static_cast<natRefPointer<FieldDecl>>(decl);
		})
		.where([](natRefPointer<FieldDecl> const& decl) -> nBool
		{
			return decl;
		});
}

Linq<Valued<natRefPointer<MethodDecl>>> ClassDecl::GetMethods() const noexcept
{
	return from(GetDecls())
		.select([](natRefPointer<Decl> const& decl)
		{
			return static_cast<natRefPointer<MethodDecl>>(decl);
		})
		.where([](natRefPointer<MethodDecl> const& decl) -> nBool
		{
			return decl;
		});
}

Linq<Valued<natRefPointer<ClassDecl>>> ClassDecl::GetBases() const noexcept
{
	// TODO
	std::terminate();
}

ImportDecl::~ImportDecl()
{
}

EmptyDecl::~EmptyDecl()
{
}

ConstructorDecl::~ConstructorDecl()
{
}

DestructorDecl::~DestructorDecl()
{
}

#define DECL(Type, Base) void Type##Decl::Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) { visitor->Visit##Type##Decl(ForkRef<Type##Decl>()); }
#include "Basic/DeclDef.h"
