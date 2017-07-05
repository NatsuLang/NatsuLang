#include "Diagnostic.h"
#include "Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

nString Diag::DiagnosticsEngine::convertArgumentToString(nuInt index)
{
	const auto& arg = m_Arguments[index];
	switch (arg.first)
	{
	case ArgumentType::String:
		return *arg.second.String;
	case ArgumentType::SInt:
		return nStrView{ std::to_string(arg.second.SInt).data() };
	case ArgumentType::UInt:
		return nStrView{ std::to_string(arg.second.UInt).data() };
	case ArgumentType::TokenType:
		return GetTokenName(arg.second.TokenType);
	case ArgumentType::IdentifierInfo:
		return arg.second.IdentifierInfo->GetName();
	default:
		return "(Broken argument)";
	}
}
