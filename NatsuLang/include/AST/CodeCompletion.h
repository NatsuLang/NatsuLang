#pragma once
#include <unordered_set>
#include <natLinq.h>
#include "ASTNode.h"

namespace NatsuLang
{
	class CodeCompleteResult
	{
	public:
		explicit CodeCompleteResult(NatsuLib::Linq<NatsuLib::Valued<ASTNodePtr>> const& results);

		NatsuLib::Linq<NatsuLib::Valued<ASTNodePtr>> GetResults() const noexcept;

	private:
		std::unordered_set<ASTNodePtr> m_Results;
	};

	struct ICodeCompleter
		: NatsuLib::natRefObj
	{
		virtual ~ICodeCompleter();

		virtual void HandleCodeCompleteResult(CodeCompleteResult const& result) = 0;
	};
}
