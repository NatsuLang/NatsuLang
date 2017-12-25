#pragma once
#include <natMisc.h>
#include <natRefObj.h>
#include <natLinq.h>
#include <unordered_set>
#include "AST/DeclBase.h"

namespace NatsuLang::Semantic
{
	enum class ScopeFlags : nuShort
	{
		None						= 0x0000,

		FunctionScope				= 0x0001,
		BreakableScope				= 0x0002,
		ContinuableScope			= 0x0004,
		DeclarableScope				= 0x0008,
		ControlScope				= 0x0010,
		ClassScope					= 0x0020,
		BlockScope					= 0x0040,
		FunctionPrototypeScope		= 0x0080,
		FunctionDeclarationScope	= 0x0100,
		SwitchScope					= 0x0200,
		TryScope					= 0x0400,
		EnumScope					= 0x0800,
		CompoundStmtScope			= 0x1000,
		UnsafeScope					= 0x2000,
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(ScopeFlags);

	class Scope
		: public NatsuLib::natRefObjImpl<Scope>
	{
	public:
		Scope(NatsuLib::natRefPointer<Scope> parent, ScopeFlags flags)
		{
			SetFlags(std::move(parent), flags);
		}

		ScopeFlags GetFlags() const noexcept
		{
			return m_Flags;
		}

		void SetFlags(ScopeFlags flags) noexcept
		{
			SetFlags(m_Parent, flags);
		}

		void SetFlags(NatsuLib::natRefPointer<Scope> parent, ScopeFlags flags) noexcept;

		void AddFlags(ScopeFlags flags) noexcept;
		void RemoveFlags(ScopeFlags flags) noexcept;

		nBool HasFlags(ScopeFlags flags) const noexcept;

		nuInt GetDepth() const noexcept
		{
			return m_Depth;
		}

		NatsuLib::natRefPointer<Scope> GetParent() const noexcept
		{
			return m_Parent;
		}

		NatsuLib::natWeakRefPointer<Scope> GetBreakParent() const noexcept
		{
			return m_BreakParent;
		}

		NatsuLib::natWeakRefPointer<Scope> GetContinueParent() const noexcept
		{
			return m_ContinueParent;
		}

		NatsuLib::natWeakRefPointer<Scope> GetBlockParent() const noexcept
		{
			return m_BlockParent;
		}

		NatsuLib::natWeakRefPointer<Scope> GetFunctionParent() const noexcept
		{
			return m_FunctionParent;
		}

		void AddDecl(Declaration::DeclPtr decl)
		{
			m_Decls.emplace(std::move(decl));
		}

		void RemoveDecl(Declaration::DeclPtr const& decl)
		{
			m_Decls.erase(decl);
		}

		NatsuLib::Linq<NatsuLib::Valued<Declaration::DeclPtr>> GetDecls() const noexcept
		{
			return from(m_Decls);
		}

		std::size_t GetDeclCount() const noexcept
		{
			return m_Decls.size();
		}

		Declaration::DeclContext* GetEntity() const noexcept
		{
			return m_Entity;
		}

		void SetEntity(Declaration::DeclContext* value) noexcept
		{
			m_Entity = value;
		}

	private:
		NatsuLib::natRefPointer<Scope> m_Parent;
		ScopeFlags m_Flags;
		nuInt m_Depth;
		NatsuLib::natWeakRefPointer<Scope> m_BreakParent, m_ContinueParent, m_BlockParent, m_FunctionParent;
		std::unordered_set<Declaration::DeclPtr> m_Decls;
		Declaration::DeclContext* m_Entity;
	};
}
