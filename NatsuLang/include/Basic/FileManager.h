#pragma once
#include <natVFS.h>
#include <unordered_map>

namespace NatsuLang
{
	class FileManager
	{
	public:
		FileManager();

		NatsuLib::natVFS& GetVFS() noexcept;
		NatsuLib::natRefPointer<NatsuLib::IRequest> GetFile(nStrView uri, nBool cacheFailure = true);
		NatsuLib::natRefPointer<NatsuLib::IRequest> GetFile(NatsuLib::Uri const& uri, nBool cacheFailure = true);

	private:
		NatsuLib::natVFS m_VFS;
		std::unordered_map<nStrView, NatsuLib::natRefPointer<NatsuLib::IRequest>> m_CachedFiles;
		nuInt m_FileLookups, m_FileCacheMisses;
	};
}
