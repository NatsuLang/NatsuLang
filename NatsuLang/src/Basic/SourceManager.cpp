#include "Basic/SourceManager.h"
#include "Basic/FileManager.h"
#include <natEncoding.h>

using namespace NatsuLib;
using namespace NatsuLang;

nuInt SourceManager::GetFileID(nStrView uri)
{
	return GetFileID(Uri{ uri });
}

nuInt SourceManager::GetFileID(Uri const& uri)
{
	auto iter = m_FileIDMap.find(uri);
	if (iter != m_FileIDMap.end())
	{
		return iter->second;
	}

	const auto freeID = getFreeID();
	const auto ret = m_FileContentMap.emplace(std::piecewise_construct,
		std::make_tuple(freeID),
		std::make_tuple(std::in_place_index<0>, uri));

	if (!ret.second)
	{
		return 0;
	}

	nBool succeed;
	std::tie(iter, succeed) = m_FileIDMap.emplace(uri, freeID);

	if (succeed)
	{
		return freeID;
	}

	m_FileContentMap.erase(ret.first);
	return 0;
}

nStrView SourceManager::FindFileUri(nuInt fileID) const
{
	if (const auto iter = std::find_if(m_FileIDMap.cbegin(), m_FileIDMap.cend(), [fileID](std::pair<Uri, nuInt> const& pair)
	{
		return pair.second == fileID;
	}); iter != m_FileIDMap.cend())
	{
		return iter->first.GetUnderlyingString();
	}

	return {};
}

std::pair<nBool, nStrView> SourceManager::GetFileContent(nuInt fileID, StringType encoding)
{
	if (!fileID)
	{
		return { false, {} };
	}

	const auto iter = m_FileContentMap.find(fileID);
	// 无效的ID？
	if (iter == m_FileContentMap.end())
	{
		return { false, {} };
	}

	// 文件已经缓存？
	if (iter->second.index() == 1)
	{
		return { true, std::get<1>(iter->second) };
	}

	// 文件未被缓存，加载它并返回
	const auto request = m_FileManager.GetFile(std::get<0>(iter->second));
	if (!request)
	{
		return { false, {} };
	}
	const auto responce = request->GetResponse();
	if (!responce)
	{
		return { false, {} };
	}
	const auto stream = responce->GetResponseStream();
	if (!stream)
	{
		return { false, {} };
	}

	std::vector<nByte> fileContent;
	nByte buffer[1024];
	while (true)
	{
		const auto readBytes = stream->ReadBytes(buffer, sizeof buffer);
		if (!readBytes)
		{
			break;
		}
		fileContent.insert(fileContent.end(), buffer, buffer + readBytes);
	}

	iter->second.emplace<1>(RuntimeEncoding<nString::UsingStringType>::Encode(fileContent.data(), fileContent.size(), encoding));

	return { true, std::get<1>(iter->second) };
}

nuInt SourceManager::getFreeID() const noexcept
{
	if (m_FileContentMap.empty())
	{
		return 1;
	}

	return m_FileContentMap.rbegin()->first + 1;
}
