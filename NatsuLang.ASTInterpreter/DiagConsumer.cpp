#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;

Interpreter::InterpreterDiagConsumer::InterpreterDiagConsumer(Interpreter& interpreter)
	: m_Interpreter{ interpreter }, m_Errored{ false }
{
}

Interpreter::InterpreterDiagConsumer::~InterpreterDiagConsumer()
{
}

void Interpreter::InterpreterDiagConsumer::HandleDiagnostic(Diag::DiagnosticsEngine::Level level,
	Diag::DiagnosticsEngine::Diagnostic const& diag)
{
	m_Errored |= Diag::DiagnosticsEngine::IsUnrecoverableLevel(level);

	nuInt levelId;

	switch (level)
	{
	case Diag::DiagnosticsEngine::Level::Ignored:
	case Diag::DiagnosticsEngine::Level::Note:
	case Diag::DiagnosticsEngine::Level::Remark:
		levelId = natLog::Msg;
		break;
	case Diag::DiagnosticsEngine::Level::Warning:
		levelId = natLog::Warn;
		break;
	case Diag::DiagnosticsEngine::Level::Error:
	case Diag::DiagnosticsEngine::Level::Fatal:
	default:
		levelId = natLog::Err;
		break;
	}

	m_Interpreter.m_Logger.Log(levelId, diag.GetDiagMessage());

	const auto loc = diag.GetSourceLocation();
	if (loc.GetFileID())
	{
		const auto fileUri = m_Interpreter.m_SourceManager.FindFileUri(loc.GetFileID());
		const auto[line, range] = m_Interpreter.m_Preprocessor.GetLexer()->GetLine(loc);
		if (range.IsValid())
		{
			m_Interpreter.m_Logger.Log(levelId, u8"在文件 \"{0}\"，第 {1} 行："_nv, fileUri.empty() ? u8"未知"_nv : fileUri, line + 1);
			m_Interpreter.m_Logger.Log(levelId, nStrView{ range.GetBegin().GetPos(), range.GetEnd().GetPos() });
			nString indentation(u8' ', loc.GetPos() - range.GetBegin().GetPos());
			m_Interpreter.m_Logger.Log(levelId, u8"{0}^"_nv, indentation);
		}
	}
}
