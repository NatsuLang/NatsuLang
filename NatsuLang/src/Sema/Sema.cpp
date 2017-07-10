#include "Sema/Sema.h"
#include "Lex/Preprocessor.h"

using namespace NatsuLang::Semantic;

Sema::Sema(Preprocessor& preprocessor)
	: m_Preprocessor{ preprocessor }, m_Diag{ preprocessor.GetDiag() }, m_SourceManager{ preprocessor.GetSourceManager() }
{
}

Sema::~Sema()
{
}
