#include "Diagnostic.h"
#include "Identifier.h"
#include "CharInfo.h"

using namespace NatsuLib;
using namespace NatsuLang;

nString Diag::DiagnosticsEngine::convertArgumentToString(nuInt index) const
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

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(const nString* string) const
{
	Argument arg;
	arg.String = string;
	m_Diags.m_Arguments.emplace_back(ArgumentType::String, arg);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(nInt sInt) const
{
	Argument arg;
	arg.SInt = sInt;
	m_Diags.m_Arguments.emplace_back(ArgumentType::SInt, arg);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(nuInt uInt) const
{
	Argument arg;
	arg.UInt = uInt;
	m_Diags.m_Arguments.emplace_back(ArgumentType::UInt, arg);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(Token::TokenType tokenType) const
{
	Argument arg;
	arg.TokenType = tokenType;
	m_Diags.m_Arguments.emplace_back(ArgumentType::TokenType, arg);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(const Identifier::IdentifierInfo* identifierInfo) const
{
	Argument arg;
	arg.IdentifierInfo = identifierInfo;
	m_Diags.m_Arguments.emplace_back(ArgumentType::TokenType, arg);
	return *this;
}

nString Diag::DiagnosticsEngine::Diagnostic::GetDiagMessage() const
{
	nString result{};
	result.Reserve(m_StoredDiagMessage.size());

	auto pRead = m_StoredDiagMessage.begin();
	const auto pEnd = m_StoredDiagMessage.end();
	for (; pRead != pEnd;)
	{
		if (*pRead != '{')
		{
			result.Append(*pRead);
		}
		else
		{
			nuInt tmpIndex = 0;
			while (++pRead != pEnd && CharInfo::IsWhitespace(*pRead)) {}
			while (++pRead != pEnd && CharInfo::IsDigit(*pRead))
			{
				tmpIndex = tmpIndex * 10 + *pRead - '0';
			}
			while (++pRead != pEnd && CharInfo::IsWhitespace(*pRead)) {}
			if (pRead == pEnd || *pRead != '}')
			{
				nat_Throw(natErrException, NatErr_InvalidArg, "Format of diag message is wrong.");
			}
			result.Append(m_Diag->convertArgumentToString(tmpIndex));
		}
		++pRead;
	}

	return result;
}

Diag::DiagnosticsEngine::DiagnosticBuilder Diag::DiagnosticsEngine::Report(DiagID id, SourceLocation sourceLocation)
{
	auto desc = m_TextProvider->GetText(id);
	return { *this };
}

void Diag::DiagnosticConsumer::BeginSourceFile(const Preprocessor* /*pp*/)
{
}

void Diag::DiagnosticConsumer::EndSourceFile()
{
}

void Diag::DiagnosticConsumer::Finish()
{
}
