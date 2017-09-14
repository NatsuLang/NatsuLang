#pragma once

#include <AST/ASTContext.h>
#include <AST/ASTConsumer.h>
#include <AST/Expression.h>
#include <Lex/Lexer.h>
#include <Lex/Preprocessor.h>
#include <Basic/Diagnostic.h>
#include <Basic/FileManager.h>
#include <Basic/SourceManager.h>
#include <Parse/Parser.h>

using namespace NatsuLib;
using namespace NatsuLang;

#include <catch.hpp>

class IDMap
	: public natRefObjImpl<IDMap, Misc::TextProvider<Diag::DiagnosticsEngine::DiagID>>
{
public:
	nString GetText(Diag::DiagnosticsEngine::DiagID id) override
	{
		switch (id)
		{
		case Diag::DiagnosticsEngine::DiagID::ErrUndefinedIdentifier:
			return u8"Undefined identifier {0}.";
		case Diag::DiagnosticsEngine::DiagID::ErrMultiCharInLiteral:
			return u8"Multiple char in literal.";
		case Diag::DiagnosticsEngine::DiagID::ErrUnexpectEOF:
			return u8"Unexpect eof.";
		default:
			return u8"(Error text)";
		}
	}
};

class TestDiagConsumer
	: public natRefObjImpl<TestDiagConsumer, Diag::DiagnosticConsumer>
{
public:
	void HandleDiagnostic(Diag::DiagnosticsEngine::Level level, Diag::DiagnosticsEngine::Diagnostic const& diag) override
	{
		if (level >= Diag::DiagnosticsEngine::Level::Error)
		{
			FAIL(diag.GetDiagMessage().data());
		}
		else if (level == Diag::DiagnosticsEngine::Level::Warning)
		{
			WARN(diag.GetDiagMessage().data());
		}
		else
		{
			INFO(diag.GetDiagMessage().data());
		}
	}
};

class TestAstConsumer
	: public natRefObjImpl<TestAstConsumer, ASTConsumer>
{
public:
	void Initialize(ASTContext& context) override
	{
		static_cast<void>(context);
	}

	void HandleTranslationUnit(ASTContext& context) override
	{
		static_cast<void>(context);
	}

	nBool HandleTopLevelDecl(Linq<Valued<Declaration::DeclPtr>> const& decls) override
	{
		for (auto&& decl : decls)
		{
			if (auto namedDecl = static_cast<natRefPointer<Declaration::NamedDecl>>(decl))
			{
				m_NamedDecls.emplace(namedDecl->GetIdentifierInfo()->GetName(), std::move(namedDecl));
			}
			else
			{
				m_NoNameDecls.emplace(std::move(decl));
			}
		}

		return true;
	}

	natRefPointer<Declaration::NamedDecl> GetNamedDecl(nStrView name) const noexcept
	{
		const auto iter = m_NamedDecls.find(name);
		if (iter != m_NamedDecls.cend())
		{
			return iter->second;
		}

		return nullptr;
	}

	Linq<Valued<Declaration::DeclPtr>> GetNoNameDecls() const noexcept
	{
		return from(m_NoNameDecls);
	}

private:
	std::unordered_map<nStrView, natRefPointer<Declaration::NamedDecl>> m_NamedDecls;
	std::unordered_set<Declaration::DeclPtr> m_NoNameDecls;
};
