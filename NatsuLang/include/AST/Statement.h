#pragma once
#include "StmtBase.h"
#include "DeclBase.h"
#include <natContainer.h>

namespace NatsuLang::Declaration
{
	class LabelDecl;
}

namespace NatsuLang::Expression
{
	class Expr;
}

namespace NatsuLang::Statement
{
	using ExprPtr = NatsuLib::natRefPointer<Expression::Expr>;

	class DeclStmt
		: public Stmt
	{
	public:
		using DeclPtr = NatsuLib::natRefPointer<Declaration::Decl>;
		using DeclEnumerable = NatsuLib::Linq<DeclPtr>;

		explicit DeclStmt(DeclEnumerable decls = NatsuLib::from_empty<DeclPtr>(), SourceLocation start = {}, SourceLocation end = {})
			: Stmt{ DeclStmtClass, start, end }, m_Decls{ std::begin(decls), std::end(decls) }
		{
		}

		~DeclStmt();

		DeclEnumerable const& GetDecls() const noexcept
		{
			return from(m_Decls);
		}

		void SetDecls(DeclEnumerable decls) noexcept
		{
			m_Decls.assign(std::cbegin(decls), std::cend(decls));
		}

	private:
		std::vector<DeclPtr> m_Decls;
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
		explicit CompoundStmt(StmtEnumerable const& stmts = NatsuLib::from_empty<StmtPtr>(), SourceLocation start = {}, SourceLocation end = {})
			: Stmt{ CompoundStmtClass, start, end }, m_Stmts{ std::begin(stmts), std::end(stmts) }
		{
		}

		~CompoundStmt();

		StmtEnumerable GetChildrens() override;
		void SetStmts(StmtEnumerable const& stmts);

	private:
		std::vector<StmtPtr> m_Stmts;
	};

	class SwitchCase
		: public Stmt
	{
	public:
		explicit SwitchCase(StmtType type, SourceLocation start = {}, SourceLocation end = {})
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

		virtual StmtPtr GetSubStmt() = 0;

	private:
		NatsuLib::natRefPointer<SwitchCase> m_NextSwitchCase;
	};

	class SwitchStmt
		: public Stmt
	{
	public:
		explicit SwitchStmt(ExprPtr cond)
			: Stmt{ SwitchStmtClass }, m_Cond{ std::move(cond) }
		{
		}

		~SwitchStmt();

		StmtPtr GetBody() const noexcept
		{
			return m_Body;
		}

		void SetBody(StmtPtr value, SourceLocation loc = {}) noexcept
		{
			m_Body = std::move(value);
			SetEndLoc(loc);
		}

		NatsuLib::natRefPointer<SwitchCase> GetSwitchCaseList() const noexcept
		{
			return m_FirstSwitchCase;
		}

		void SetSwitchCaseList(NatsuLib::natRefPointer<SwitchCase> switchCase) noexcept
		{
			m_FirstSwitchCase = std::move(switchCase);
		}

		void AddSwitchCase(NatsuLib::natRefPointer<SwitchCase> switchCase) noexcept
		{
			assert(!switchCase->GetNextSwitchCase());
			switchCase->SetNextSwitchCase(std::move(m_FirstSwitchCase));
			m_FirstSwitchCase = std::move(switchCase);
		}

		StmtEnumerable GetChildrens() override;

	private:
		ExprPtr m_Cond;
		StmtPtr m_Body;
		NatsuLib::natRefPointer<SwitchCase> m_FirstSwitchCase;
	};

	class CaseStmt
		: public SwitchCase
	{
	public:
		CaseStmt(ExprPtr expr, SourceLocation start, SourceLocation end)
			: SwitchCase{ CaseStmtClass, start, end }, m_Expr{ std::move(expr) }
		{
		}

		~CaseStmt();

		ExprPtr GetExpr() const noexcept
		{
			return m_Expr;
		}

		void SetExpr(ExprPtr expr) noexcept
		{
			m_Expr = std::move(expr);
		}

		StmtPtr GetSubStmt() override
		{
			return m_SubStmt;
		}

		void SetSubStmt(StmtPtr stmt) noexcept
		{
			m_SubStmt = std::move(stmt);
		}

	private:
		ExprPtr m_Expr;
		StmtPtr m_SubStmt;
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

		StmtPtr GetSubStmt() override
		{
			return m_SubStmt;
		}

		void SetSubStmt(StmtPtr stmt) noexcept
		{
			m_SubStmt = std::move(stmt);
		}

	private:
		StmtPtr m_SubStmt;
	};

	class LabelStmt
		: public Stmt
	{
	public:
		using LabelDeclPtr = NatsuLib::natWeakRefPointer<Declaration::LabelDecl>;

		LabelStmt(SourceLocation loc, LabelDeclPtr decl, StmtPtr subStmt)
			: Stmt{ LabelStmtClass, loc, subStmt->GetEndLoc() }, m_Decl{ std::move(decl) }
		{
		}
		~LabelStmt();

		nStrView GetName() const noexcept;

		StmtPtr GetSubStmt() const noexcept
		{
			return m_SubStmt;
		}

		void SetSubStmt(StmtPtr stmt) noexcept
		{
			m_SubStmt = std::move(stmt);
			SetEndLoc(m_SubStmt->GetEndLoc());
		}

		StmtEnumerable GetChildrens() override;

	private:
		LabelDeclPtr m_Decl;
		StmtPtr m_SubStmt;
	};

	class IfStmt
		: public Stmt
	{
	public:
		IfStmt(SourceLocation ifLoc, ExprPtr condExpr, StmtPtr thenStmt, SourceLocation elseLoc = {}, StmtPtr elseStmt = {})
			: Stmt{ IfStmtClass, ifLoc, (elseStmt ? elseStmt : thenStmt)->GetEndLoc() },
			m_Cond{ std::move(condExpr) },
			m_ElseLocation{ elseLoc },
			m_Then{ std::move(thenStmt) },
			m_Else{ std::move(elseStmt) }
		{
		}

		~IfStmt();

		ExprPtr GetCond() const noexcept
		{
			return m_Cond;
		}

		void SetCond(ExprPtr value) noexcept
		{
			m_Cond = std::move(value);
		}

		StmtPtr GetThen() const noexcept
		{
			return m_Then;
		}

		void SetThen(StmtPtr value) noexcept
		{
			m_Then = std::move(value);
		}

		StmtPtr GetElse() const noexcept
		{
			return m_Else;
		}

		void SetElse(StmtPtr value) noexcept
		{
			m_Else = std::move(value);
			SetEndLoc((m_Else ? m_Else : m_Then)->GetEndLoc());
		}

		StmtEnumerable GetChildrens() override;

	private:
		ExprPtr m_Cond;
		SourceLocation m_ElseLocation;
		StmtPtr m_Then, m_Else;
	};

	class WhileStmt
		: public Stmt
	{
	public:
		WhileStmt(SourceLocation loc, ExprPtr cond, StmtPtr body)
			: Stmt{ WhileStmtClass, loc, body->GetEndLoc() }, m_Cond{ std::move(cond) }, m_Body{ std::move(body) }
		{
		}

		~WhileStmt();

		ExprPtr GetCond() const noexcept
		{
			return m_Cond;
		}

		void SetCond(ExprPtr value) noexcept
		{
			m_Cond = std::move(value);
		}

		StmtPtr GetBody() const noexcept
		{
			return m_Body;
		}

		void SetBody(StmtPtr value) noexcept
		{
			m_Body = std::move(value);
			SetEndLoc(m_Body->GetEndLoc());
		}

		StmtEnumerable GetChildrens() override;

	private:
		ExprPtr m_Cond;
		StmtPtr m_Body;
	};

	class DoStmt
		: public Stmt
	{
	public:
		DoStmt(StmtPtr body, ExprPtr cond, SourceLocation doLoc, SourceLocation whileLoc, SourceLocation endLoc)
			: Stmt{ DoStmtClass, doLoc, endLoc }, m_Body{ std::move(body) }, m_Cond{ std::move(cond) }, m_WhileLoc{ whileLoc }
		{
		}

		~DoStmt();

		ExprPtr GetCond() const noexcept
		{
			return m_Cond;
		}

		void SetCond(ExprPtr value) noexcept
		{
			m_Cond = std::move(value);
		}

		StmtPtr GetBody() const noexcept
		{
			return m_Body;
		}

		void SetBody(StmtPtr value) noexcept
		{
			m_Body = std::move(value);
		}

		SourceLocation GetWhileLocation() const noexcept
		{
			return m_WhileLoc;
		}

		void SetWhileLocation(SourceLocation loc) noexcept
		{
			m_WhileLoc = loc;
		}

		StmtEnumerable GetChildrens() override;

	private:
		StmtPtr m_Body;
		ExprPtr m_Cond;
		SourceLocation m_WhileLoc;
	};

	class ForStmt
		: public Stmt
	{
	public:
		ForStmt(StmtPtr init, ExprPtr cond, ExprPtr inc, StmtPtr body, SourceLocation forLoc, SourceLocation lpLoc, SourceLocation rpLoc)
			: Stmt{ ForStmtClass, forLoc, body->GetEndLoc() },
			m_Init{ std::move(init) },
			m_Cond{ std::move(cond) },
			m_Inc{ std::move(inc) },
			m_Body{ std::move(body) },
			m_LParenLoc{ lpLoc },
			m_RParenLoc{ rpLoc }
		{
		}

		~ForStmt();

		StmtPtr GetInit() const noexcept
		{
			return m_Init;
		}

		void SetInit(StmtPtr value) noexcept
		{
			m_Init = std::move(value);
		}

		ExprPtr GetCond() const noexcept
		{
			return m_Cond;
		}

		void SetCond(ExprPtr value) noexcept
		{
			m_Cond = std::move(value);
		}

		ExprPtr GetInc() const noexcept
		{
			return m_Inc;
		}

		void SetInc(ExprPtr value) noexcept
		{
			m_Inc = std::move(value);
		}

		StmtPtr GetBody() const noexcept
		{
			return m_Body;
		}

		void SetBody(StmtPtr value) noexcept
		{
			m_Body = std::move(value);
			SetEndLoc(m_Body->GetEndLoc());
		}

		SourceLocation GetLParenLoc() const noexcept
		{
			return m_LParenLoc;
		}

		void SetLParenLoc(SourceLocation loc) noexcept
		{
			m_LParenLoc = loc;
		}

		SourceLocation GetRParenLoc() const noexcept
		{
			return m_RParenLoc;
		}

		void SetRParenLoc(SourceLocation loc) noexcept
		{
			m_RParenLoc = loc;
		}

		StmtEnumerable GetChildrens() override;

	private:
		StmtPtr m_Init;
		ExprPtr m_Cond;
		ExprPtr m_Inc;
		StmtPtr m_Body;
		SourceLocation m_LParenLoc, m_RParenLoc;
	};

	class GotoStmt
		: public Stmt
	{
	public:
		GotoStmt(NatsuLib::natRefPointer<LabelStmt> label, SourceLocation gotoLoc, SourceLocation labelLoc)
			: Stmt{ GotoStmtClass, gotoLoc, labelLoc }, m_Label{ std::move(label) }
		{
		}

		~GotoStmt();

		NatsuLib::natRefPointer<LabelStmt> GetLabel() const noexcept
		{
			return m_Label;
		}

		void SetLabel(NatsuLib::natRefPointer<LabelStmt> value) noexcept
		{
			m_Label = std::move(value);
		}

	private:
		NatsuLib::natRefPointer<LabelStmt> m_Label;
	};

	class ContinueStmt
		: public Stmt
	{
	public:
		explicit ContinueStmt(SourceLocation loc)
			: Stmt{ ContinueStmtClass, loc, loc }
		{
		}

		~ContinueStmt();
	};

	class BreakStmt
		: public Stmt
	{
	public:
		explicit BreakStmt(SourceLocation loc)
			: Stmt{ BreakStmtClass, loc, loc }
		{
		}

		~BreakStmt();
	};

	class ReturnStmt
		: public Stmt
	{
	public:
		ReturnStmt(SourceLocation loc, ExprPtr retExpr);
		~ReturnStmt();

		ExprPtr GetReturnExpr() const noexcept
		{
			return m_RetExpr;
		}

		void SetReturnExpr(ExprPtr value) noexcept;

		StmtEnumerable GetChildrens() override;

	private:
		ExprPtr m_RetExpr;
	};
}
