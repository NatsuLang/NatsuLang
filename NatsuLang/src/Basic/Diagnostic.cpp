﻿#include "Basic/Diagnostic.h"
#include "Basic/Identifier.h"
#include "Basic/CharInfo.h"

using namespace NatsuLib;
using namespace NatsuLang;

Diag::DiagnosticsEngine::DiagnosticsEngine(natRefPointer<Misc::TextProvider<DiagID>> idMap, natRefPointer<DiagnosticConsumer> consumer)
	: m_IDMap{ std::move(idMap) }, m_Consumer{ std::move(consumer) }, m_Enabled{ true }, m_CurrentID{ DiagID::Invalid },
	  m_CurrentRequiredArgs{}
{
}

Diag::DiagnosticsEngine::~DiagnosticsEngine()
{
}

void Diag::DiagnosticsEngine::Clear() noexcept
{
	m_CurrentID = DiagID::Invalid;
	m_CurrentDiagDesc.Clear();
	m_Arguments.clear();
	m_CurrentRequiredArgs = 0;
	m_CurrentSourceRange = {};
}

nBool Diag::DiagnosticsEngine::EmitDiag()
{
	if (m_Enabled && m_CurrentID != DiagID::Invalid && m_Arguments.size() >= m_CurrentRequiredArgs)
	{
		m_Consumer->HandleDiagnostic(getDiagLevel(m_CurrentID), Diagnostic{ this, std::move(m_CurrentDiagDesc) });
		Clear();
		return true;
	}

	return false;
}

void Diag::DiagnosticsEngine::EnableDiag(nBool value) noexcept
{
	m_Enabled = value;
}

nBool Diag::DiagnosticsEngine::IsDiagEnabled() const noexcept
{
	return m_Enabled;
}

// TODO: 由 DiagnosticConsumer 决定如何解释参数
nString Diag::DiagnosticsEngine::convertArgumentToString(nuInt index) const
{
	const auto& arg = m_Arguments[index];
	switch (arg.index())
	{
	case 0:
		return std::get<0>(arg);
	case 1:
		return nString{ std::get<1>(arg) };
	case 2:
		return nStrView{ std::to_string(std::get<2>(arg)).data() };
	case 3:
		return nStrView{ std::to_string(std::get<3>(arg)).data() };
	case 4:
		return Lex::GetTokenName(std::get<4>(arg));
	case 5:
		return std::get<5>(arg)->GetName();
	case 6:
	{
		const auto& range = std::get<6>(arg);
		// TODO: 完成 SourceRange 的诊断信息输出
		return u8"(SourceRange)"_ns;
	}
	default:
		assert(!"Invalid argument");
		return "(Invalid argument)";
	}
}

Diag::DiagnosticsEngine::DiagnosticBuilder::~DiagnosticBuilder()
{
	m_Diags.EmitDiag();
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(nString string) const
{
	m_Diags.m_Arguments.emplace_back(std::in_place_index<0>, std::move(string));
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(nChar Char) const
{
	m_Diags.m_Arguments.emplace_back(std::in_place_index<1>, Char);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(nInt sInt) const
{
	m_Diags.m_Arguments.emplace_back(std::in_place_index<2>, sInt);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(nuInt uInt) const
{
	m_Diags.m_Arguments.emplace_back(std::in_place_index<3>, uInt);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(Lex::TokenType tokenType) const
{
	m_Diags.m_Arguments.emplace_back(std::in_place_index<4>, tokenType);
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(natRefPointer<Identifier::IdentifierInfo> identifierInfo) const
{
	m_Diags.m_Arguments.emplace_back(std::in_place_index<5>, std::move(identifierInfo));
	return *this;
}

const Diag::DiagnosticsEngine::DiagnosticBuilder& Diag::DiagnosticsEngine::DiagnosticBuilder::AddArgument(SourceRange const& sourceRange) const
{
	m_Diags.m_Arguments.emplace_back(std::in_place_index<6>, sourceRange);
	return *this;
}

nString Diag::DiagnosticsEngine::Diagnostic::GetDiagMessage() const
{
	assert(m_Diag->m_CurrentID != DiagID::Invalid && m_Diag->m_Arguments.size() >= m_Diag->m_CurrentRequiredArgs);

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
			++pRead;
			while (pRead != pEnd && CharInfo::IsWhitespace(*pRead))
			{
				++pRead;
			}

			while (pRead != pEnd && CharInfo::IsDigit(*pRead))
			{
				tmpIndex = tmpIndex * 10 + *pRead++ - '0';
			}

			while (pRead != pEnd && CharInfo::IsWhitespace(*pRead))
			{
				++pRead;
			}

			if (pRead == pEnd || *pRead != '}')
			{
				nat_Throw(natErrException, NatErr_InvalidArg, "Expect '}'.");
			}
			assert(tmpIndex < m_Diag->m_Arguments.size());
			result.Append(m_Diag->convertArgumentToString(tmpIndex));
		}
		++pRead;
	}

	return result;
}

std::size_t Diag::DiagnosticsEngine::GetArgumentCount() const noexcept
{
	return m_Arguments.size();
}

Diag::DiagnosticsEngine::Argument const& Diag::DiagnosticsEngine::GetArgument(std::size_t i) const
{
	return m_Arguments[i];
}

Diag::DiagnosticsEngine::DiagnosticBuilder Diag::DiagnosticsEngine::Report(DiagID id, SourceRange sourcerange)
{
	// 之前的诊断还未处理？
	if (m_CurrentID != DiagID::Invalid)
	{
		EmitDiag();
	}

	// 为了强异常安全
	m_CurrentDiagDesc = m_IDMap->GetText(id);

	m_CurrentID = id;
	m_CurrentRequiredArgs = getDiagArgCount(id);
	m_CurrentSourceRange = sourcerange;

	return { *this };
}

Diag::DiagnosticConsumer::~DiagnosticConsumer()
{
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

Diag::DiagException::DiagException(nStrView src, nStrView file, nuInt line, DiagnosticsEngine::DiagID diagId)
	: BaseException(src, file, line, u8"Exception with DiagID infomation."), m_Id{ diagId }
{
}

Diag::DiagException::DiagException(std::exception_ptr nestedException, nStrView src, nStrView file, nuInt line, DiagnosticsEngine::DiagID diagId)
	: BaseException(std::move(nestedException), src, file, line, u8"Exception with DiagID infomation."), m_Id{ diagId }
{
}

Diag::DiagException::~DiagException()
{
}

Diag::DiagnosticsEngine::DiagID Diag::DiagException::GetDiagId() const noexcept
{
	return m_Id;
}
