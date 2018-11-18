#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;

Interpreter::InterpreterASTConsumer::InterpreterASTConsumer(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
}

Interpreter::InterpreterASTConsumer::~InterpreterASTConsumer()
{
}

void Interpreter::InterpreterASTConsumer::Initialize(ASTContext& /*context*/)
{
}

void Interpreter::InterpreterASTConsumer::HandleTranslationUnit(ASTContext& context)
{
	const auto iter = m_NamedDecls.find(u8"Main"_nv);
	natRefPointer<Declaration::FunctionDecl> mainDecl;
	if (iter == m_NamedDecls.cend() || !((mainDecl = iter->second)))
	{
		nat_Throw(InterpreterException, u8"无法找到名为 Main 的函数"_nv);
	}

	if (mainDecl->Decl::GetType() != Declaration::Decl::Function)
	{
		nat_Throw(InterpreterException, u8"找到了名为 Main 的方法，但需要一个函数"_nv);
	}

	m_Interpreter.m_Visitor.Visit(mainDecl->GetBody());
}

nBool Interpreter::InterpreterASTConsumer::HandleTopLevelDecl(Linq<Valued<Declaration::DeclPtr>> const& decls)
{
	for (auto&& decl : decls)
	{
		if (const auto varDecl = decl.Cast<Declaration::VarDecl>(); varDecl && HasAllFlags(varDecl->GetStorageClass(), Specifier::StorageClass::Const))
		{
			continue;
		}

		if (auto namedDecl = decl.Cast<Declaration::NamedDecl>())
		{
			m_NamedDecls.emplace(namedDecl->GetIdentifierInfo()->GetName(), std::move(namedDecl));
		}
		else
		{
			m_UnnamedDecls.emplace_back(std::move(decl));
		}
	}

	return true;
}
