#include <AST/ASTContext.h>
#include <AST/ASTConsumer.h>
#include <AST/Expression.h>
#include <Lex/Lexer.h>
#include <Lex/Preprocessor.h>
#include <Basic/Diagnostic.h>
#include <Basic/FileManager.h>
#include <Basic/SourceManager.h>
#include <Parse/Parser.h>
#include <natConsole.h>

using namespace NatsuLib;
using namespace NatsuLang;
using namespace Lex;
using namespace Diag;

class IDMap
	: public natRefObjImpl<IDMap, Misc::TextProvider<DiagnosticsEngine::DiagID>>
{
public:
	nString GetText(DiagnosticsEngine::DiagID id) override
	{
		switch (id)
		{
		case DiagnosticsEngine::DiagID::ErrUndefinedIdentifier:
			return u8"Undefined identifier {0}.";
		case DiagnosticsEngine::DiagID::ErrMultiCharInLiteral:
			return u8"Multiple char in literal.";
		case DiagnosticsEngine::DiagID::ErrUnexpectEOF:
			return u8"Unexpect eof.";
		default:
			return u8"(Error text)";
		}
	}
};

class JitDiagConsumer
	: public natRefObjImpl<JitDiagConsumer, DiagnosticConsumer>
{
public:
	explicit JitDiagConsumer(natConsole& console)
		: m_Console{ console }
	{
	}

	void HandleDiagnostic(DiagnosticsEngine::Level /*level*/, DiagnosticsEngine::Diagnostic const& diag) override
	{
		m_Console.WriteLineErr(diag.GetDiagMessage());
	}

private:
	natConsole& m_Console;
};

class JitAstConsumer
	: public natRefObjImpl<JitAstConsumer, ASTConsumer>
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

constexpr char TestCode[] = u8R"(
def Increase : (arg : int = 1) -> int
{
	return 1 + arg;
}
)";

int main()
{

}
