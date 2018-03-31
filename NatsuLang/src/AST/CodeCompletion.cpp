#include "AST/CodeCompletion.h"

using namespace NatsuLib;
using namespace NatsuLang;

CodeCompleteResult::CodeCompleteResult(Linq<Valued<ASTNodePtr>> const& results)
	: m_Results(results.begin(), results.end())
{
}

Linq<Valued<ASTNodePtr>> CodeCompleteResult::GetResults() const noexcept
{
	return from(m_Results);
}

ICodeCompleter::~ICodeCompleter()
{
}
