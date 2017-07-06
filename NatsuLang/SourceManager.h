#pragma once
#include "natString.h"
#include <map>
#include <unordered_map>
#include <variant>

namespace NatsuLang
{
	namespace Diag
	{
		class DiagnosticsEngine;
	}
	
	class FileManager;

	class SourceManager
	{
	public:
		explicit SourceManager(Diag::DiagnosticsEngine& diagnosticsEngine, FileManager& fileManager, NatsuLib::StringType encoding = nString::UsingStringType)
			: m_DiagnosticsEngine{ diagnosticsEngine }, m_FileManager { fileManager }, m_Encoding{ encoding }
		{
		}

		Diag::DiagnosticsEngine& GetDiagnosticsEngine() const noexcept
		{
			return m_DiagnosticsEngine;
		}

		FileManager& GetFileManager() const noexcept
		{
			return m_FileManager;
		}

		NatsuLib::StringType GetEncoding() const noexcept
		{
			return m_Encoding;
		}

		NatsuLib::StringType SetEncoding(NatsuLib::StringType newEncoding) noexcept
		{
			return std::exchange(m_Encoding, newEncoding);
		}

		nuInt GetFileID(nStrView uri);
		std::pair<nBool, nStrView> GetFileContent(nuInt fileID);

	private:
		Diag::DiagnosticsEngine& m_DiagnosticsEngine;
		FileManager& m_FileManager;
		NatsuLib::StringType m_Encoding;
		// Key: 文件URI, Value: 文件ID
		std::unordered_map<nStrView, nuInt> m_FileIDMap;
		// Key: 文件ID, Value: （未加载过）文件URI/（已加载过）文件内容
		std::map<nuInt, std::variant<nStrView, nString>> m_FileContentMap;

		nuInt getFreeID() const noexcept;
	};
}
