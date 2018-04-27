#pragma once
#include <natString.h>
#include <natVFS.h>
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
		explicit SourceManager(Diag::DiagnosticsEngine& diagnosticsEngine, FileManager& fileManager)
			: m_DiagnosticsEngine{ diagnosticsEngine }, m_FileManager{ fileManager }
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

		nuInt GetFileID(nStrView uri);
		nuInt GetFileID(NatsuLib::Uri const& uri);
		nStrView FindFileUri(nuInt fileID) const;
		std::pair<nBool, nStrView> GetFileContent(nuInt fileID, NatsuLib::StringType encoding = nString::UsingStringType);

	private:
		Diag::DiagnosticsEngine& m_DiagnosticsEngine;
		FileManager& m_FileManager;
		// Key: 文件URI, Value: 文件ID
		std::unordered_map<NatsuLib::Uri, nuInt> m_FileIDMap;
		// Key: 文件ID, Value: （未加载过）文件URI/（已加载过）文件内容
		std::map<nuInt, std::variant<NatsuLib::Uri, nString>> m_FileContentMap;

		nuInt getFreeID() const noexcept;
	};
}
