#pragma once
#include <natMisc.h>

namespace NatsuLang
{
	namespace Diag
	{
		class DiagnosticsEngine;
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

	private:
		Preprocessor& m_Preprocessor;
		Diag::DiagnosticsEngine& m_Diag;
		SourceManager& m_SourceManager;
	};
}
