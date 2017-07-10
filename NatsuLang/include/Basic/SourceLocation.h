#pragma once
#include <natType.h>

namespace NatsuLang
{
	class SourceLocation
	{
	public:
		constexpr SourceLocation(nuInt fileID = 0, nuInt line = 0, nuInt col = 0) noexcept
			: m_FileID{ fileID }, m_Line{ line }, m_Column{ col }
		{
		}

		constexpr nBool IsValid() const noexcept
		{
			return m_FileID;
		}

		constexpr nuInt GetFileID() const noexcept
		{
			return m_FileID;
		}

		constexpr nBool HasLineInfo() const noexcept
		{
			return m_Line;
		}

		constexpr nuInt GetLineInfo() const noexcept
		{
			return m_Line;
		}

		constexpr nBool HasColumnInfo() const noexcept
		{
			return m_Column;
		}

		constexpr nuInt GetColumnInfo() const noexcept
		{
			return m_Column;
		}

	private:
		nuInt m_FileID, m_Line, m_Column;
	};

	constexpr nBool operator<(SourceLocation const& loc1, SourceLocation const& loc2) noexcept
	{
		if (loc1.GetFileID() != loc2.GetFileID())
		{
			return loc1.GetFileID() < loc2.GetFileID();
		}

		if (loc1.GetLineInfo() != loc2.GetLineInfo())
		{
			return loc1.GetLineInfo() < loc2.GetLineInfo();
		}

		return loc1.GetColumnInfo() < loc2.GetColumnInfo();
	}
}
