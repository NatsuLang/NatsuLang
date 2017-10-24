#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;

Interpreter::InterpreterDiagIdMap::InterpreterDiagIdMap(natRefPointer<TextReader<StringType::Utf8>> const& reader)
{
	using DiagIDUnderlyingType = std::underlying_type_t<Diag::DiagnosticsEngine::DiagID>;

	std::unordered_map<nStrView, Diag::DiagnosticsEngine::DiagID> idNameMap;
	for (auto id = static_cast<DiagIDUnderlyingType>(Diag::DiagnosticsEngine::DiagID::Invalid) + 1;
		id < static_cast<DiagIDUnderlyingType>(Diag::DiagnosticsEngine::DiagID::EndOfDiagID); ++id)
	{
		if (auto idName = Diag::DiagnosticsEngine::GetDiagIDName(static_cast<Diag::DiagnosticsEngine::DiagID>(id)))
		{
			idNameMap.emplace(idName, static_cast<Diag::DiagnosticsEngine::DiagID>(id));
		}
	}

	nString diagIDName;
	while (true)
	{
		auto curLine = reader->ReadLine();

		if (diagIDName.IsEmpty())
		{
			if (curLine.IsEmpty())
			{
				break;
			}

			diagIDName = std::move(curLine);
		}
		else
		{
			const auto iter = idNameMap.find(diagIDName);
			if (iter == idNameMap.cend())
			{
				// TODO: 报告错误的 ID
				break;
			}

			if (!m_IdMap.try_emplace(iter->second, std::move(curLine)).second)
			{
				// TODO: 报告重复的 ID
			}

			diagIDName.Clear();
		}
	}
}

Interpreter::InterpreterDiagIdMap::~InterpreterDiagIdMap()
{
}

nString Interpreter::InterpreterDiagIdMap::GetText(Diag::DiagnosticsEngine::DiagID id)
{
	const auto iter = m_IdMap.find(id);
	return iter == m_IdMap.cend() ? u8"(No available text)"_nv : iter->second.GetView();
}
