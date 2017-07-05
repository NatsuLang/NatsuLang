#pragma once
#include <vector>
#include <natString.h>
#include "Token.h"
#include "TextProvider.h"
#include "SourceLocation.h"

namespace NatsuLang
{
	class Preprocessor;
}

namespace NatsuLang::Diag
{
	enum class DiagID
	{
#define DIAG(ID) ID,
#include "DiagDef.h"
	};

	class DiagnosticsEngine
	{
	public:
		enum class Level
		{
			Ignored,
			Note,
			Remark,
			Warning,
			Error,
			Fatal
		};

		enum class ArgumentType
		{
			String,
			SInt,
			UInt,
			TokenType,
			IdentifierInfo
		};

		DiagnosticsEngine();
		~DiagnosticsEngine();

	private:
		union Argument
		{
			const nString* String;
			nInt SInt;
			nuInt UInt;
			Token::TokenType TokenType;
			const Identifier::IdentifierInfo* IdentifierInfo;
		};

		std::vector<std::pair<ArgumentType, Argument>> m_Arguments;
		NatsuLib::natRefPointer<Misc::TextProvider<DiagID>> m_TextProvider;

		nString convertArgumentToString(nuInt index) const;

	public:
		class DiagnosticBuilder
		{
		public:
			constexpr DiagnosticBuilder(DiagnosticsEngine& diags)
				: m_Diags{ diags }
			{
			}

			const DiagnosticBuilder& AddArgument(const nString* string) const;
			const DiagnosticBuilder& AddArgument(nInt sInt) const;
			const DiagnosticBuilder& AddArgument(nuInt uInt) const;
			const DiagnosticBuilder& AddArgument(Token::TokenType tokenType) const;
			const DiagnosticBuilder& AddArgument(const Identifier::IdentifierInfo* identifierInfo) const;

		private:
			DiagnosticsEngine& m_Diags;
		};

		class Diagnostic
		{
		public:
			Diagnostic(const DiagnosticsEngine* diag, nString msg)
				: m_Diag{ diag }, m_StoredDiagMessage{ std::move(msg) }
			{
			}

			const DiagnosticsEngine* GetDiag() const noexcept
			{
				return m_Diag;
			}

			nuInt GetArgCount() const noexcept
			{
				return m_Diag->m_Arguments.size();
			}

			nString GetDiagMessage() const;

		private:
			const DiagnosticsEngine* const m_Diag;
			nString m_StoredDiagMessage;
		};

	public:
		DiagnosticBuilder Report(DiagID id, SourceLocation sourceLocation = {});
	};

	struct DiagnosticConsumer
		: NatsuLib::natRefObjImpl<DiagnosticConsumer>
	{
		virtual void BeginSourceFile(const Preprocessor* pp);
		virtual void EndSourceFile();
		virtual void Finish();
		virtual void HandleDiagnostic(DiagnosticsEngine::Level level, DiagnosticsEngine::Diagnostic const& diag) = 0;
	};
}
