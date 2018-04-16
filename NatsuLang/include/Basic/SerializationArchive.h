#pragma once
#include <natRefObj.h>
#include <natString.h>
#include "SourceLocation.h"

namespace NatsuLang
{
	struct ISerializationArchiveReader
		: NatsuLib::natRefObj
	{
		virtual ~ISerializationArchiveReader();

		virtual nBool ReadSourceLocation(nStrView key, SourceLocation& out) = 0;
		virtual nBool ReadString(nStrView key, nString& out) = 0;
		virtual nBool ReadInteger(nStrView key, nuLong& out, std::size_t widthHint = 8) = 0;
		virtual nBool ReadBool(nStrView key, nBool& out);
		virtual nBool ReadFloat(nStrView key, nDouble& out, std::size_t widthHint = 8) = 0;

		template <typename T>
		std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, nBool> ReadNumType(nStrView key, T& out)
		{
			if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
			{
				nuLong value;
				const auto ret = ReadInteger(key, value, sizeof(T));
				out = static_cast<T>(value);
				return ret;
			}
			else
			{
				nDouble value;
				const auto ret = ReadFloat(key, value, sizeof(T));
				out = static_cast<T>(value);
				return ret;
			}
		}

		// 利用这些方法读取复杂属性
		virtual nBool StartReadingEntry(nStrView key, nBool isArray = false) = 0;
		virtual nBool NextReadingElement() = 0;
		virtual std::size_t GetEntryElementCount() = 0;
		virtual void EndReadingEntry() = 0;
	};

	struct ISerializationArchiveWriter
		: NatsuLib::natRefObj
	{
		virtual ~ISerializationArchiveWriter();

		virtual void WriteSourceLocation(nStrView key, SourceLocation const& value) = 0;
		virtual void WriteString(nStrView key, nStrView value) = 0;
		virtual void WriteInteger(nStrView key, nuLong value, std::size_t widthHint = 8) = 0;
		virtual void WriteBool(nStrView key, nBool value);
		virtual void WriteFloat(nStrView key, nDouble value, std::size_t widthHint = 8) = 0;

		template <typename T>
		std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>> WriteNumType(nStrView key, T value)
		{
			if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
			{
				WriteInteger(key, static_cast<nuLong>(value), sizeof(T));
			}
			else
			{
				WriteFloat(key, static_cast<nDouble>(value), sizeof(T));
			}
		}

		// 利用这些方法写入复杂属性
		virtual void StartWritingEntry(nStrView key, nBool isArray = false) = 0;
		virtual void NextWritingElement() = 0;
		virtual void EndWritingEntry() = 0;
	};
}
