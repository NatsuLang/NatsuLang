#pragma once
#include <natString.h>

namespace NatsuLang
{
	class SourceLocation
	{
	public:
		constexpr SourceLocation(nuInt fileID = 0, nStrView::const_iterator pos = nullptr) noexcept
			: m_FileID{ fileID }, m_Pos{ pos }
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

		constexpr void SetFileID(nuInt value) noexcept
		{
			m_FileID = value;
		}

		constexpr nStrView::const_iterator GetPos() const noexcept
		{
			return m_Pos;
		}

		constexpr void SetPos(nStrView::const_iterator pos) noexcept
		{
			m_Pos = pos;
		}

	private:
		nuInt m_FileID;
		nStrView::const_iterator m_Pos;
	};

	constexpr nBool operator<(SourceLocation const& loc1, SourceLocation const& loc2) noexcept
	{
		if (loc1.GetFileID() != loc2.GetFileID())
		{
			return loc1.GetFileID() < loc2.GetFileID();
		}

		return loc1.GetPos() < loc2.GetPos();
	}

	constexpr nBool operator==(SourceLocation const& loc1, SourceLocation const& loc2) noexcept
	{
		return loc1.GetFileID() == loc2.GetFileID() && loc1.GetPos() == loc2.GetPos();
	}

	constexpr nBool operator!=(SourceLocation const& loc1, SourceLocation const& loc2) noexcept
	{
		return !(loc1 == loc2);
	}

	class SourceRange
	{
	public:
		constexpr SourceRange() = default;

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
