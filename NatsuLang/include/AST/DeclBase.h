#pragma once
#include <natMisc.h>
#include <natRefObj.h>
#include "Basic/SourceLocation.h"

namespace NatsuLang::Declaration
{
	enum class IdentifierNamespace
	{
		None	= 0x00,

		Label	= 0x01,
		Tag		= 0x02,
		Type	= 0x04,
		Member	= 0x08,
		Module	= 0x10,
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(IdentifierNamespace);

	class DeclContext;

	class Decl
		: public NatsuLib::natRefObjImpl<Decl>
	{
	public:
		enum class Type
		{
#define DECL(Derived, Base) Derived,
#define ABSTRACT_DECL(Decl)
#define DECL_RANGE(Base, Start, End) \
		First##Base = Start, Last##Base = End,
#define LAST_DECL_RANGE(Base, Start, End) \
		First##Base = Start, Last##Base = End
#include "Basic/DeclDef.h"
		};

		static DeclContext* castToDeclContext(const Decl* decl);
		static Decl* castFromDeclContext(const DeclContext* declContext);

		Type GetType() const noexcept
		{
			return m_Type;
		}

		const char* GetTypeName() const noexcept;

		SourceLocation GetLocation() const noexcept
		{
			return m_Location;
		}

		void SetLocation(SourceLocation loc) noexcept
		{
			m_Location = loc;
		}

		DeclContext* GetContext() const noexcept
		{
			return m_Context;
		}

		Decl* GetNextDeclInContext() const noexcept
		{
			return m_NextDeclInContext;
		}

	protected:
		explicit Decl(Type type, DeclContext* context = nullptr, SourceLocation loc = {}) noexcept
			: m_NextDeclInContext{ nullptr }, m_Type{ type }, m_Context{ context }, m_Location{ loc }
		{
		}

		Decl* m_NextDeclInContext;

	private:
		Type m_Type;
		DeclContext* m_Context;
		SourceLocation m_Location;
	};

	class DeclContext
	{
	public:
		~DeclContext() = default;

	protected:
		constexpr explicit DeclContext(Decl::Type type) noexcept
			: m_Type{ type }, m_Decls{ nullptr, nullptr }
		{
		}

	public:
		constexpr Decl::Type GetType() const noexcept
		{
			return m_Type;
		}

		const char* GetTypeName() const noexcept;

	private:
		Decl::Type m_Type;
		mutable NatsuLib::Range<Decl*> m_Decls;
	};
}
