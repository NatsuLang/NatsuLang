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
		const auto [succeed, fileContent] = m_Interpreter.m_SourceManager.GetFileContent(loc.GetFileID());
		if (const auto line = loc.GetLineInfo(); succeed && line)
		{
			size_t offset{};
			for (nuInt i = 1; i < line; ++i)
			{
				offset = fileContent.Find(Environment::GetNewLine(), static_cast<ptrdiff_t>(offset));
				if (offset == nStrView::npos)
				{
					// TODO: 无法定位到源文件
					return;
				}

				offset += Environment::GetNewLine().GetSize();
			}

			const auto nextNewLine = fileContent.Find(Environment::GetNewLine(), static_cast<ptrdiff_t>(offset));
			const auto column = loc.GetColumnInfo();
			offset += column ? column - 1 : 0;
			if (nextNewLine <= offset)
			{
				// TODO: 无法定位到源文件
				return;
			}

			m_Interpreter.m_Logger.Log(levelId, fileContent.Slice(static_cast<ptrdiff_t>(offset), nextNewLine == nStrView::npos ? -1 : static_cast<ptrdiff_t>(nextNewLine)));
			m_Interpreter.m_Logger.Log(levelId, u8"^"_nv);
		}
	}

	m_Errored |= Diag::DiagnosticsEngine::IsUnrecoverableLevel(level);
}
