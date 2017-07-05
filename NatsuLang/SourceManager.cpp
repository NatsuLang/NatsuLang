#include "SourceManager.h"
#include "FileManager.h"
#include <natEncoding.h>

using namespace NatsuLib;
using namespace NatsuLang;

nuInt SourceManager::GetFileID(nStrView uri)
{
	auto iter = m_FileIDMap.find(uri);
	if (iter != m_FileIDMap.end())
	{
		return iter->second;
	}

	const auto freeID = getFreeID();
	auto ret = m_FileContentMap.emplace(freeID, std::variant<nStrView, nString>{ std::in_place_index<0>, uri });
	if (!ret.second)
	{
		return 0;
	}

	nBool succeed;
	tie(iter, succeed) = m_FileIDMap.emplace(uri, freeID);
	
	if (succeed)
	{
		return freeID;
	}

	m_FileContentMap.erase(ret.first);
	return 0;
}

std::pair<nBool, nStrView> SourceManager::GetFileContent(nuInt fileID)
{
	if (!fileID)
	{
		return { false,{} };
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
	
	iter->second = std::variant<nStrView, nString>{ std::in_place_index<1>,
		RuntimeEncoding<nString::UsingStringType>::Encode(fileContent.data(), fileContent.size(), m_Encoding) };

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
