﻿#pragma once
#include <vector>
#include <natException.h>
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
		: NatsuLib::nonmovable
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

		static constexpr const char* GetDiagLevelName(Level level) noexcept
		{
			switch (level)
			{
			case Level::Ignored:
				return "Ignored";
			case Level::Note:
				return "Note";
			case Level::Remark:
				return "Remark";
			case Level::Warning:
				return "Warning";
			case Level::Error:
				return "Error";
			case Level::Fatal:
				return "Fatal";
			default:
				assert(!"Invalid level.");
				return "";
			}
		}

		static constexpr nBool IsUnrecoverableLevel(Level level) noexcept
		{
			return level >= Level::Error;
		}

		enum class DiagID
		{
			Invalid,

#define DIAG(ID, Level, ArgCount) ID,
#include "DiagDef.h"

			EndOfDiagID
		};

		static constexpr const char* GetDiagIDName(DiagID id) noexcept
		{
			switch (id)
			{
#define DIAG(ID, Level, ArgCount) case DiagID::ID: return #ID;
#include "DiagDef.h"

			case DiagID::Invalid:
			case DiagID::EndOfDiagID:
			default:
				return nullptr;
			}
		}

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

		void EnableDiag(nBool value) noexcept;
		nBool IsDiagEnabled() const noexcept;

		NatsuLib::natRefPointer<DiagnosticConsumer> GetDiagConsumer() const noexcept
		{
			return m_Consumer;
		}

	private:
		using Argument = std::variant<nString, nChar, nInt, nuInt, Lex::TokenType, NatsuLib::natRefPointer<Identifier::IdentifierInfo>, SourceRange>;

		std::vector<Argument> m_Arguments;
		NatsuLib::natRefPointer<Misc::TextProvider<DiagID>> m_IDMap;
		NatsuLib::natRefPointer<DiagnosticConsumer> m_Consumer;

		nBool m_Enabled;

		DiagID m_CurrentID;
		nuInt m_CurrentRequiredArgs;
		nString m_CurrentDiagDesc;
		SourceRange m_CurrentSourceRange;

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
			: nonmovable
		{
		public:
			constexpr DiagnosticBuilder(DiagnosticsEngine& diags)
				: m_Diags{ diags }
			{
			}

			~DiagnosticBuilder();

			const DiagnosticBuilder& AddArgument(nString string) const;
			const DiagnosticBuilder& AddArgument(nChar Char) const;
			const DiagnosticBuilder& AddArgument(nInt sInt) const;
			const DiagnosticBuilder& AddArgument(nuInt uInt) const;
			const DiagnosticBuilder& AddArgument(Lex::TokenType tokenType) const;
			const DiagnosticBuilder& AddArgument(NatsuLib::natRefPointer<Identifier::IdentifierInfo> identifierInfo) const;
			const DiagnosticBuilder& AddArgument(SourceRange const& sourceRange) const;

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

			SourceRange GetSourceRange() const noexcept
			{
				return m_Diag->m_CurrentSourceRange;
			}

			nString GetDiagMessage() const;

		private:
			const DiagnosticsEngine* const m_Diag;
			nString m_StoredDiagMessage;
		};

	public:
		std::size_t GetArgumentCount() const noexcept;
		Argument const& GetArgument(std::size_t i) const;
		DiagnosticBuilder Report(DiagID id, SourceRange sourcerange = {});
	};

	struct DiagnosticConsumer
		: NatsuLib::natRefObjImpl<DiagnosticConsumer>
	{
		~DiagnosticConsumer();

		virtual void BeginSourceFile(const Preprocessor* pp);
		virtual void EndSourceFile();
		virtual void Finish();
		virtual void HandleDiagnostic(DiagnosticsEngine::Level level, DiagnosticsEngine::Diagnostic const& diag) = 0;
	};

	class DiagException
		: public NatsuLib::natException
	{
	public:
		typedef NatsuLib::natException BaseException;

		DiagException(nStrView src, nStrView file, nuInt line, DiagnosticsEngine::DiagID diagId);
		DiagException(std::exception_ptr nestedException, nStrView src, nStrView file, nuInt line, DiagnosticsEngine::DiagID diagId);

		template <typename... Args>
		DiagException(nStrView src, nStrView file, nuInt line, DiagnosticsEngine::DiagID diagId, nStrView desc, Args&&... args)
			: BaseException(src, file, line, desc, std::forward<Args>(args)...), m_Id{ diagId }
		{
		}

		template <typename... Args>
		DiagException(std::exception_ptr nestedException, nStrView src, nStrView file, nuInt line, DiagnosticsEngine::DiagID diagId, nStrView desc, Args&&... args)
			: BaseException(nestedException, src, file, line, desc, std::forward<Args>(args)...), m_Id{ diagId }
		{
		}

		~DiagException();

		DiagnosticsEngine::DiagID GetDiagId() const noexcept;

	private:
		DiagnosticsEngine::DiagID m_Id;
	};
}
