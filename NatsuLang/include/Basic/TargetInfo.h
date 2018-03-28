#pragma once
#include <natEnvironment.h>

namespace NatsuLang
{
	class TargetInfo
	{
	public:
		constexpr TargetInfo(NatsuLib::Environment::Endianness endianness, std::size_t pointerSize, std::size_t pointerAlign)
			: m_Endianness{ endianness }, m_PointerSize{ pointerSize }, m_PointerAlign{ pointerAlign }
		{
		}

		constexpr NatsuLib::Environment::Endianness GetEndianness() const noexcept
		{
			return m_Endianness;
		}

		constexpr void SetEndianness(NatsuLib::Environment::Endianness value) noexcept
		{
			m_Endianness = value;
		}

		constexpr std::size_t GetPointerSize() const noexcept
		{
			return m_PointerSize;
		}

		constexpr void SetPointerSize(std::size_t value) noexcept
		{
			m_PointerSize = value;
		}

		constexpr std::size_t GetPointerAlign() const noexcept
		{
			return m_PointerAlign;
		}

		constexpr void SetPointerAlign(std::size_t value) noexcept
		{
			m_PointerAlign = value;
		}

	private:
		NatsuLib::Environment::Endianness m_Endianness;
		std::size_t m_PointerSize, m_PointerAlign;
	};
}
