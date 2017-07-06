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
	struct DiagnosticConsumer;

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

		enum class DiagID
		{
			Invalid,
#define DIAG(ID, Level, ArgCount) ID,
#include "DiagDef.h"
		};

		enum class ArgumentType
		{
			String,
			Char,
			SInt,
			UInt,
			TokenType,
			IdentifierInfo
		};

		DiagnosticsEngine(NatsuLib::natRefPointer<Misc::TextProvider<DiagID>> idMap, NatsuLib::natRefPointer<DiagnosticConsumer> consumer);
		~DiagnosticsEngine();

		void Clear() noexcept;
		nBool EmitDiag();

	private:
		using Argument = std::variant<nString, nChar, nInt, nuInt, Token::TokenType, NatsuLib::natRefPointer<Identifier::IdentifierInfo>>;

		std::vector<std::pair<ArgumentType, Argument>> m_Arguments;
		NatsuLib::natRefPointer<Misc::TextProvider<DiagID>> m_IDMap;
		NatsuLib::natRefPointer<DiagnosticConsumer> m_Consumer;

		DiagID m_CurrentID;
		nuInt m_CurrentRequiredArgs;
		nString m_CurrentDiagDesc;
		SourceLocation m_CurrentSourceLocation;

		nString convertArgumentToString(nuInt index) const;

		static constexpr Level getDiagLevel(DiagID id) noexcept
		{
			switch (id)
			{
#define DIAG(ID, Level, ArgCount) case DiagID::ID: return Level;
#include "DiagDef.h"
			default:
				assert(!"There should never be reached.");
				return Level::Fatal;
			}
		}

		static constexpr nuInt getDiagArgCount(DiagID id) noexcept
		{
			switch (id)
			{
#define DIAG(ID, Level, ArgCount) case DiagID::ID: return ArgCount;
#include "DiagDef.h"
			default:
				assert(!"There should never be reached.");
				return 0;
			}
		}

	public:
		class DiagnosticBuilder
		{
		public:
			constexpr DiagnosticBuilder(DiagnosticsEngine& diags)
				: m_Diags{ diags }
			{
			}

			const DiagnosticBuilder& AddArgument(nString string) const;
			const DiagnosticBuilder& AddArgument(nChar Char) const;
			const DiagnosticBuilder& AddArgument(nInt sInt) const;
			const DiagnosticBuilder& AddArgument(nuInt uInt) const;
			const DiagnosticBuilder& AddArgument(Token::TokenType tokenType) const;
			const DiagnosticBuilder& AddArgument(NatsuLib::natRefPointer<Identifier::IdentifierInfo> identifierInfo) const;

		private:
			DiagnosticsEngine& m_Diags;
		};

		class Diagnostic
		{
		public:
			Diagnostic(const DiagnosticsEngine* diag, nString msg)
				: m_Diag{ diag }, m_StoredDiagMessage{ std::move(msg) }
			{
				assert(diag);
			}

			const DiagnosticsEngine* GetDiag() const noexcept
			{
				return m_Diag;
			}

			nuInt GetArgCount() const noexcept
			{
				return static_cast<nuInt>(m_Diag->m_Arguments.size());
			}

			SourceLocation GetSourceLocation() const noexcept
			{
				return m_Diag->m_CurrentSourceLocation;
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
