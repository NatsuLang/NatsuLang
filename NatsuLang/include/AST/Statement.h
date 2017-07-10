#pragma once
#include "StmtBase.h"
#include "DeclBase.h"
#include <natContainer.h>

namespace NatsuLang::Statement
{
	class Expr;

	class DeclStmt
		: public Stmt
	{
	public:
		explicit DeclStmt(NatsuLib::Linq<Declaration::Decl> decls = NatsuLib::from_empty<Declaration::Decl>(), SourceLocation start = {}, SourceLocation end = {})
			: Stmt{ DeclStmtClass, start, end }, m_Decls{ decls }
		{
		}

		~DeclStmt();

		NatsuLib::Linq<Declaration::Decl> const& GetDecls() const noexcept
		{
			return m_Decls;
		}

		void SetDecls(NatsuLib::Linq<Declaration::Decl> decls) noexcept
		{
			m_Decls = std::move(decls);
		}

	private:
		NatsuLib::Linq<Declaration::Decl> m_Decls;
	};

	class NullStmt
		: public Stmt
	{
	public:
		explicit NullStmt(SourceLocation loc = {})
			: Stmt{ NullStmtClass, loc, loc }
		{
		}
		~NullStmt();

		void SetStartLoc(SourceLocation loc) noexcept override;
		void SetEndLoc(SourceLocation loc) noexcept override;

	private:
		SourceLocation m_Location;
	};

	class CompoundStmt
		: public Stmt
	{
	public:
		explicit CompoundStmt(NatsuLib::Container<Stmt*> const& stmts = {}, SourceLocation start = {}, SourceLocation end = {})
			: Stmt{ CompoundStmtClass, start, end }
		{
			if (stmts.HasContainer())
			{
				m_Stmts.insert(m_Stmts.end(), std::cbegin(stmts), std::cend(stmts));
			}
		}

		~CompoundStmt();

		NatsuLib::Linq<Stmt> GetChildrens() override;
		void SetStmts(NatsuLib::Container<Stmt*> const& stmts);

	private:
		std::vector<Stmt*> m_Stmts;
	};

	class SwitchCase
		: public Stmt
	{
	public:
		explicit SwitchCase(Type type, SourceLocation start = {}, SourceLocation end = {})
			: Stmt{ type, start, end }
		{
		}
		~SwitchCase();

		NatsuLib::natRefPointer<SwitchCase> GetNextSwitchCase() const noexcept
		{
			return m_NextSwitchCase;
		}

		void SetNextSwitchCase(NatsuLib::natRefPointer<SwitchCase> value) noexcept
		{
			m_NextSwitchCase = std::move(value);
		}

		virtual NatsuLib::natRefPointer<Stmt> GetSubStmt() = 0;

	private:
		NatsuLib::natRefPointer<SwitchCase> m_NextSwitchCase;
	};

	class CaseStmt
		: public SwitchCase
	{
	public:
		CaseStmt(NatsuLib::natRefPointer<Expr> expr, SourceLocation start, SourceLocation end)
			: SwitchCase{ CaseStmtClass, start, end }, m_Expr{ std::move(expr) }
		{
		}

		~CaseStmt();

		NatsuLib::natRefPointer<Expr> GetExpr() const noexcept
		{
			return m_Expr;
		}

		void SetExpr(NatsuLib::natRefPointer<Expr> expr) noexcept
		{
			m_Expr = std::move(expr);
		}

		NatsuLib::natRefPointer<Stmt> GetSubStmt() override
		{
			return m_SubStmt;
		}

		void SetSubStmt(NatsuLib::natRefPointer<Stmt> stmt) noexcept
		{
			m_SubStmt = std::move(stmt);
		}

	private:
		NatsuLib::natRefPointer<Expr> m_Expr;
		NatsuLib::natRefPointer<Stmt> m_SubStmt;
	};

	class DefaultStmt
		: public SwitchCase
	{
	public:
		DefaultStmt(SourceLocation start, SourceLocation end, NatsuLib::natRefPointer<Stmt> subStmt)
			: SwitchCase{ DefaultStmtClass, start, end }, m_SubStmt{ std::move(subStmt) }
		{
		}

		~DefaultStmt();

		NatsuLib::natRefPointer<Stmt> GetSubStmt() override
		{
			return m_SubStmt;
		}

		void SetSubStmt(NatsuLib::natRefPointer<Stmt> stmt) noexcept
		{
			m_SubStmt = std::move(stmt);
		}

	private:
		NatsuLib::natRefPointer<Stmt> m_SubStmt;
	};

}
