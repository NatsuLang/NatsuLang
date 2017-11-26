#pragma once
#include <natMisc.h>
#include <natRefObj.h>
#include <natLinq.h>
#include "ASTNode.h"
#include "Basic/SourceLocation.h"

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
	using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
}

namespace NatsuLang::Declaration
{
	enum class IdentifierNamespace
	{
		None		= 0x00,

		Label		= 0x01,
		Tag			= 0x02,
		Type		= 0x04,
		Member		= 0x08,
		Module		= 0x10,
		Ordinary	= 0x20,
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(IdentifierNamespace);

	class DeclContext;
	class NamedDecl;

	class Decl
		: public NatsuLib::natRefObjImpl<Decl, ASTNode>
	{
		friend class DeclContext;

	public:
		enum DeclType
		{
#define DECL(Derived, Base) Derived,
//#define ABSTRACT_DECL(Decl)
#define DECL_RANGE(Base, Start, End) \
		First##Base = Start, Last##Base = End,
#define LAST_DECL_RANGE(Base, Start, End) \
		First##Base = Start, Last##Base = End
#include "Basic/DeclDef.h"
		};

		static DeclContext* CastToDeclContext(const Decl* decl);
		static Decl* CastFromDeclContext(const DeclContext* declContext);

		DeclType GetType() const noexcept
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

		void SetContext(DeclContext* dc) noexcept
		{
			m_Context = dc;
		}

		NatsuLib::natRefPointer<Decl> GetNextDeclInContext() const noexcept
		{
			return m_NextDeclInContext;
		}

		nBool IsFunction() const noexcept;

	protected:
		explicit Decl(DeclType type, DeclContext* context = nullptr, SourceLocation loc = {}) noexcept
			: m_NextDeclInContext{ nullptr }, m_Type{ type }, m_Context{ context }, m_Location{ loc }
		{
		}

		NatsuLib::natRefPointer<Decl> m_NextDeclInContext;

	private:
		DeclType m_Type;
		DeclContext* m_Context;
		SourceLocation m_Location;

		void SetNextDeclInContext(NatsuLib::natRefPointer<Decl> value) noexcept;
	};

	using DeclPtr = NatsuLib::natRefPointer<Decl>;

	class DeclContext
	{
	protected:
		constexpr explicit DeclContext(Decl::DeclType type) noexcept
			: m_Type{ type }
		{
		}

		~DeclContext() = default;

	public:
		Decl::DeclType GetType() const noexcept
		{
			return m_Type;
		}

		const char* GetTypeName() const noexcept;

		NatsuLib::Linq<NatsuLib::Valued<DeclPtr>> GetDecls() const;

		void AddDecl(DeclPtr decl);
		void RemoveDecl(DeclPtr const& decl);
		nBool ContainsDecl(DeclPtr const& decl);

		NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<NamedDecl>>> Lookup(
			NatsuLib::natRefPointer<Identifier::IdentifierInfo> const& info) const;

	private:
		Decl::DeclType m_Type;
		mutable DeclPtr m_FirstDecl, m_LastDecl;

		class DeclIterator
		{
		public:
			typedef std::forward_iterator_tag iterator_category;
			typedef DeclPtr value_type;
			typedef std::ptrdiff_t difference_type;
			typedef std::add_lvalue_reference_t<value_type> reference;
			typedef std::add_pointer_t<value_type> pointer;

			explicit DeclIterator(DeclPtr firstDecl = {});

			DeclPtr operator*() const noexcept;
			DeclIterator& operator++() & noexcept;
			nBool operator==(DeclIterator const& other) const noexcept;
			nBool operator!=(DeclIterator const& other) const noexcept;

		private:
			DeclPtr m_Current;
		};

		virtual void OnNewDeclAdded(DeclPtr decl);
	};
}
