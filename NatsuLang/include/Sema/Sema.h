#pragma once
#include <natMisc.h>
#include <natRefObj.h>
#include <optional>
#include "Basic/SourceLocation.h"
#include "Basic/Identifier.h"

namespace NatsuLang
{
	namespace Diag
	{
		class DiagnosticsEngine;
	}

	namespace Declaration
	{
		class Decl;
	}

	class Preprocessor;
	class SourceManager;
}

namespace NatsuLang::Semantic
{
	class Sema
		: public NatsuLib::nonmovable
	{
	public:
		enum class ExpressionEvaluationContext
		{
			Unevaluated,
			DiscardedStatement,
			ConstantEvaluated,
			PotentiallyEvaluated,
			PotentiallyEvaluatedIfUsed
		};

		using ModulePathType = std::vector<std::pair<NatsuLib::natRefPointer<Identifier::IdentifierInfo>, SourceLocation>>;

		explicit Sema(Preprocessor& preprocessor);
		~Sema();

		Preprocessor& GetPreprocessor() const noexcept
		{
			return m_Preprocessor;
		}

		Diag::DiagnosticsEngine& GetDiagnosticsEngine() const noexcept
		{
			return m_Diag;
		}

		SourceManager& GetSourceManager() const noexcept
		{
			return m_SourceManager;
		}

		NatsuLib::natRefPointer<Declaration::Decl> OnModuleImport(SourceLocation startLoc, SourceLocation importLoc, ModulePathType const& path);

	private:
		Preprocessor& m_Preprocessor;
		Diag::DiagnosticsEngine& m_Diag;
		SourceManager& m_SourceManager;
	};
}
