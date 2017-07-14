#pragma once
#include <natRefObj.h>
#include "Basic/SourceLocation.h"

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
	using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
}

namespace NatsuLang::Declaration
{
	class Declarator
	{
	public:
		enum class Context
		{
			File,
			Prototype,
			TypeName,
			Member,
			Block,
			For,
			New,
			Catch,
		};

		explicit Declarator(Context context)
			: m_Context{ context }
		{
		}

		Identifier::IdPtr GetIdentifier() const noexcept
		{
			return m_Identifier;
		}

		void SetIdentifier(Identifier::IdPtr idPtr) noexcept
		{
			m_Identifier = std::move(idPtr);
		}

		SourceRange GetRange() const noexcept
		{
			return m_Range;
		}

		void SetRange(SourceRange range) noexcept
		{
			m_Range = range;
		}

		Context GetContext() const noexcept
		{
			return m_Context;
		}

		void SetContext(Context context) noexcept
		{
			m_Context = context;
		}

	private:
		Identifier::IdPtr m_Identifier;
		SourceRange m_Range;
		Context m_Context;
	};
}
