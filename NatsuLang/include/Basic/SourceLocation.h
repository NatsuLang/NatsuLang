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

	constexpr nBool operator==(SourceLocation const& loc1, SourceLocation const& loc2) noexcept
	{
		return loc1.GetFileID() == loc2.GetFileID() && loc1.GetLineInfo() == loc2.GetLineInfo() && loc1.GetColumnInfo() == loc2.GetColumnInfo();
	}

	constexpr nBool operator!=(SourceLocation const& loc1, SourceLocation const& loc2) noexcept
	{
		return !(loc1 == loc2);
	}

	class SourceRange
	{
	public:
		constexpr SourceRange() noexcept
			: m_Begin{}, m_End{}
		{
		}

		constexpr SourceRange(SourceLocation loc) noexcept
			: m_Begin{ loc }, m_End{ loc }
		{
		}

		constexpr SourceRange(SourceLocation begin, SourceLocation end) noexcept
			: m_Begin{ begin }, m_End{ end }
		{
		}

		constexpr SourceLocation GetBegin() const noexcept
		{
			return m_Begin;
		}

		void SetBegin(SourceLocation loc) noexcept
		{
			m_Begin = loc;
		}

		constexpr SourceLocation GetEnd() const noexcept
		{
			return m_End;
		}

		void SetEnd(SourceLocation loc) noexcept
		{
			m_End = loc;
		}

		constexpr nBool IsValid() const noexcept
		{
			return m_Begin.IsValid() && m_End.IsValid();
		}

	private:
		SourceLocation m_Begin, m_End;
	};

	constexpr nBool operator==(SourceRange const& range1, SourceRange const& range2) noexcept
	{
		return range1.GetBegin() == range2.GetBegin() && range1.GetEnd() == range2.GetEnd();
	}

	constexpr nBool operator!=(SourceRange const& range1, SourceRange const& range2) noexcept
	{
		return !(range1 == range2);
	}
}
