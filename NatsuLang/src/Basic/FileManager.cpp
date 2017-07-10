#include "Basic/FileManager.h"

using namespace NatsuLib;
using namespace NatsuLang;

FileManager::FileManager()
	: m_FileLookups{}, m_FileCacheMisses{}
{
}

natVFS& FileManager::GetVFS() noexcept
{
	return m_VFS;
}

natRefPointer<IRequest> FileManager::GetFile(nStrView uri, nBool cacheFailure)
{
	++m_FileLookups;
	auto iter = m_CachedFiles.find(uri);
	if (iter != m_CachedFiles.end())
	{
		return iter->second;
	}

	++m_FileCacheMisses;

	auto request = m_VFS.CreateRequest(Uri{ uri });
	if (!request && !cacheFailure)
	{
		return nullptr;
	}

	nBool succeed;
	tie(iter, succeed) = m_CachedFiles.emplace(uri, std::move(request));
	if (!succeed)
	{
		nat_Throw(natErrException, NatErr_InternalErr, "Cannot add entry.");
	}

	return iter->second;
}
