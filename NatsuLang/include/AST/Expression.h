#pragma once
#include "Declaration.h"
#include "Statement.h"
#include "OperationTypes.h"
#include <variant>

#define DEFAULT_ACCEPT_DECL void Accept(NatsuLib::natRefPointer<StmtVisitor> const& visitor) override

namespace NatsuLang
{
	class ASTContext;
	class NestedNameSpecifier;
}

namespace NatsuLang::Expression
{
	class Expr;

	using ExprPtr = NatsuLib::natRefPointer<Expr>;

	class Expr
		: public Statement::Stmt
	{
	public:
		Expr(StmtType stmtType, Type::TypePtr exprType, SourceLocation start = {}, SourceLocation end = {});
		~Expr();

		Type::TypePtr GetExprType() const noexcept
		{
			return m_ExprType;
		}

		void SetExprType(Type::TypePtr value) noexcept
		{
			m_ExprType = std::move(value);
		}

		ExprPtr IgnoreParens() noexcept;

		struct EvalResult
		{
			nBool HasSideEffects = false;
			nBool HasUndefinedBehavior = false;
			std::variant<nuLong, nDouble> Result;

			nBool GetResultAsSignedInteger(nLong& result) const noexcept;
			nBool GetResultAsBoolean(nBool& result) const noexcept;
		};

		nBool Evaluate(EvalResult& result, ASTContext& context);
		nBool EvaluateAsInt(nuLong& result, ASTContext& context);
		nBool EvaluateAsFloat(nDouble& result, ASTContext& context);

		DEFAULT_ACCEPT_DECL;

	private:
		Type::TypePtr m_ExprType;
	};

	class DeclRefExpr
		: public Expr
	{
	public:
		DeclRefExpr(NatsuLib::natRefPointer<NestedNameSpecifier> nns, NatsuLib::natRefPointer<Declaration::ValueDecl> valueDecl, SourceLocation loc, Type::TypePtr exprType)
			: Expr{ DeclRefExprClass, std::move(exprType), loc, loc }, m_NestedNameSpecifier{ std::move(nns) }, m_ValueDecl{ std::move(valueDecl) }
		{
		}

		~DeclRefExpr();

		NatsuLib::natRefPointer<NestedNameSpecifier> GetNestedNameSpecifier() const noexcept
		{
			return m_NestedNameSpecifier;
		}

		NatsuLib::natRefPointer<Declaration::ValueDecl> GetDecl() const noexcept
		{
			return m_ValueDecl;
		}

		void SetDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> value) noexcept
		{
			m_ValueDecl = std::move(value);
		}

		DEFAULT_ACCEPT_DECL;

	private:
		NatsuLib::natRefPointer<NestedNameSpecifier> m_NestedNameSpecifier;
		NatsuLib::natRefPointer<Declaration::ValueDecl> m_ValueDecl;
	};

	class IntegerLiteral
		: public Expr
	{
	public:
		IntegerLiteral(nuLong value, Type::TypePtr type, SourceLocation loc)
			: Expr{ IntegerLiteralClass, std::move(type), loc, loc }, m_Value{ value }
		{
		}

		~IntegerLiteral();

		nuLong GetValue() const noexcept
		{
			return m_Value;
		}

		void SetValue(nuLong value) noexcept
		{
			m_Value = value;
		}

		DEFAULT_ACCEPT_DECL;

	private:
		nuLong m_Value;
	};

	class CharacterLiteral
		: public Expr
	{
	public:
		CharacterLiteral(nuInt codePoint, NatsuLib::StringType charType, Type::TypePtr type, SourceLocation loc)
			: Expr{ CharacterLiteralClass, std::move(type), loc, loc }, m_CodePoint{ codePoint }, m_CharType{ charType }
		{
		}

		~CharacterLiteral();

		nuInt GetCodePoint() const noexcept
		{
			return m_CodePoint;
		}

		void SetCodePoint(nuInt value) noexcept
		{
			m_CodePoint = value;
		}

		NatsuLib::StringType GetCharType() const noexcept
		{
			return m_CharType;
		}

		void SetCharType(NatsuLib::StringType value) noexcept
		{
			m_CharType = value;
		}

		DEFAULT_ACCEPT_DECL;

	private:
		nuInt m_CodePoint;
		NatsuLib::StringType m_CharType;
	};

	class FloatingLiteral
		: public Expr
	{
	public:
		FloatingLiteral(nDouble value, Type::TypePtr type, SourceLocation loc)
			: Expr{ FloatingLiteralClass, std::move(type), loc, loc }, m_Value{ value }
		{
		}

		~FloatingLiteral();

		nDouble GetValue() const noexcept
		{
			return m_Value;
		}

		void SetValue(nDouble value) noexcept
		{
			m_Value = value;
		}

		DEFAULT_ACCEPT_DECL;

	private:
		nDouble m_Value;
	};

	class StringLiteral
		: public Expr
	{
	public:
		StringLiteral(nString value, Type::TypePtr type, SourceLocation loc)
			: Expr{ StringLiteralClass, std::move(type), loc, loc }, m_Value{ std::move(value) }
		{
		}

		~StringLiteral();

		nStrView GetValue() const noexcept
		{
			return m_Value;
		}

		void SetValue(nString str) noexcept
		{
			m_Value = std::move(str);
		}

		DEFAULT_ACCEPT_DECL;

	private:
		nString m_Value;
	};

	class BooleanLiteral
		: public Expr
	{
	public:
		BooleanLiteral(nBool value, Type::TypePtr type, SourceLocation loc)
			: Expr{ BooleanLiteralClass, std::move(type), loc, loc }, m_Value{ value }
		{
		}

		~BooleanLiteral();

		nBool GetValue() const noexcept
		{
			return m_Value;
		}

		void SetValue(nBool value) noexcept
		{
			m_Value = value;
		}

		DEFAULT_ACCEPT_DECL;

	private:
		nBool m_Value;
	};

	class ParenExpr
		: public Expr
	{
	public:
		ParenExpr(ExprPtr expr, SourceLocation start, SourceLocation end)
			: Expr{ ParenExprClass, expr->GetExprType(), start, end }, m_InnerExpr{ std::move(expr) }
		{
		}

		~ParenExpr();

		ExprPtr GetInnerExpr() const noexcept
		{
			return m_InnerExpr;
		}

		void SetInnerExpr(ExprPtr value) noexcept
		{
			m_InnerExpr = std::move(value);
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_InnerExpr;
	};

	class UnaryOperator
		: public Expr
	{
	public:
		UnaryOperator(ExprPtr operand, UnaryOperationType opcode, Type::TypePtr type, SourceLocation loc)
			: Expr{ UnaryOperatorClass, std::move(type), loc, loc }, m_Operand{ std::move(operand) }, m_Opcode{ opcode }
		{
		}

		~UnaryOperator();

		ExprPtr GetOperand() const noexcept
		{
			return m_Operand;
		}

		void SetOperand(ExprPtr value) noexcept
		{
			m_Operand = std::move(value);
		}

		UnaryOperationType GetOpcode() const noexcept
		{
			return m_Opcode;
		}

		void SetOpcode(UnaryOperationType value) noexcept
		{
			m_Opcode = value;
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_Operand;
		UnaryOperationType m_Opcode;
	};

	class ArraySubscriptExpr
		: public Expr
	{
	public:
		ArraySubscriptExpr(ExprPtr leftOperand, ExprPtr rightOperand, Type::TypePtr type, SourceLocation loc)
			: Expr{ ArraySubscriptExprClass, std::move(type), loc, loc }, m_LeftOperand{ std::move(leftOperand) }, m_RightOperand{ std::move(rightOperand) }
		{
		}

		~ArraySubscriptExpr();

		ExprPtr GetLeftOperand() const noexcept
		{
			return m_LeftOperand;
		}

		void SetLeftOperand(ExprPtr value) noexcept
		{
			m_LeftOperand = std::move(value);
		}

		ExprPtr GetRightOperand() const noexcept
		{
			return m_RightOperand;
		}

		void SetRightOperand(ExprPtr value) noexcept
		{
			m_RightOperand = std::move(value);
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_LeftOperand, m_RightOperand;
	};

	class CallExpr
		: public Expr
	{
	protected:
		CallExpr(StmtType stmtType, ExprPtr func, NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> const& args, Type::TypePtr type, SourceLocation loc)
			: Expr{ stmtType, std::move(type), loc, loc }, m_Function{ std::move(func) }, m_Args{ args.begin(), args.end() }
		{
		}

	public:
		CallExpr(ExprPtr func, NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> const& args, Type::TypePtr type, SourceLocation loc)
			: Expr{ CallExprClass, std::move(type), loc, loc }, m_Function{ std::move(func) }, m_Args{ args.begin(), args.end() }
		{
		}

		~CallExpr();

		ExprPtr GetCallee() const noexcept
		{
			return m_Function;
		}

		void SetCallee(ExprPtr value) noexcept
		{
			m_Function = std::move(value);
		}

		std::size_t GetArgCount() const noexcept
		{
			return m_Args.size();
		}

		NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> GetArgs() const noexcept;
		void SetArgs(NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> const& value);

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_Function;
		std::vector<ExprPtr> m_Args;
	};

	class MemberExpr
		: public Expr
	{
	public:
		MemberExpr(ExprPtr base, SourceLocation loc, NatsuLib::natRefPointer<Declaration::ValueDecl> memberDecl, Identifier::IdPtr name, Type::TypePtr type)
			: Expr{ MemberExprClass, std::move(type), loc, loc }, m_Base{ std::move(base) }, m_MemberDecl{ std::move(memberDecl) }, m_Name{ std::move(name) }
		{
		}

		~MemberExpr();

		ExprPtr GetBase() const noexcept
		{
			return m_Base;
		}

		void SetBase(ExprPtr value) noexcept
		{
			m_Base = std::move(value);
		}

		NatsuLib::natRefPointer<Declaration::ValueDecl> GetMemberDecl() const noexcept
		{
			return m_MemberDecl;
		}

		void SetMemberDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> value) noexcept
		{
			m_MemberDecl = std::move(value);
		}

		Identifier::IdPtr GetName() const noexcept
		{
			return m_Name;
		}

		void SetName(Identifier::IdPtr value) noexcept
		{
			m_Name = std::move(value);
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_Base;
		NatsuLib::natRefPointer<Declaration::ValueDecl> m_MemberDecl;
		Identifier::IdPtr m_Name;
	};

	class MemberCallExpr
		: public CallExpr
	{
	public:
		MemberCallExpr(ExprPtr func, NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> const& args, Type::TypePtr type, SourceLocation loc)
			: CallExpr{ MemberCallExprClass, std::move(func), args, std::move(type), loc }
		{
		}

		~MemberCallExpr();

		ExprPtr GetImplicitObjectArgument() const noexcept;

		DEFAULT_ACCEPT_DECL;
	};

	class CastExpr
		: public Expr
	{
	public:
		CastExpr(StmtType stmtType, Type::TypePtr type, CastType castType, ExprPtr operand)
			: Expr{ stmtType, std::move(type) }, m_CastType{ castType }, m_Operand{ std::move(operand) }
		{
		}

		~CastExpr();

		CastType GetCastType() const noexcept
		{
			return m_CastType;
		}

		void SetCastType(CastType value) noexcept
		{
			m_CastType = value;
		}

		ExprPtr GetOperand() const noexcept
		{
			return m_Operand;
		}

		void SetOperand(ExprPtr value) noexcept
		{
			m_Operand = std::move(value);
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		CastType m_CastType;
		ExprPtr m_Operand;
	};

	class ImplicitCastExpr
		: public CastExpr
	{
	public:
		ImplicitCastExpr(Type::TypePtr type, CastType castType, ExprPtr operand)
			: CastExpr{ ImplicitCastExprClass, std::move(type), castType, operand }
		{
			Stmt::SetStartLoc(operand->GetStartLoc());
			Stmt::SetEndLoc(operand->GetEndLoc());
		}

		~ImplicitCastExpr();

		DEFAULT_ACCEPT_DECL;
	};

	class AsTypeExpr
		: public CastExpr
	{
	public:
		AsTypeExpr(Type::TypePtr type, CastType castType, ExprPtr operand)
			: CastExpr{ AsTypeExprClass, std::move(type), castType, operand }
		{
			Stmt::SetStartLoc(operand->GetStartLoc());
			Stmt::SetEndLoc(operand->GetEndLoc());
		}

		~AsTypeExpr();

		DEFAULT_ACCEPT_DECL;
	};

	class InitListExpr
		: public Expr
	{
	public:
		InitListExpr(Type::TypePtr type, SourceLocation leftBraceLoc, std::vector<ExprPtr> initExprs, SourceLocation rightBraceLoc)
			: Expr{ InitListExprClass, std::move(type) }, m_LeftBraceLoc{ leftBraceLoc }, m_RightBraceLoc{ rightBraceLoc }, m_InitExprs{ std::move(initExprs) }
		{
			Stmt::SetStartLoc(m_LeftBraceLoc);
			Stmt::SetEndLoc(m_RightBraceLoc);
		}

		~InitListExpr();

		SourceLocation GetLeftBraceLoc() const noexcept
		{
			return m_LeftBraceLoc;
		}

		SourceLocation GetRightBraceLoc() const noexcept
		{
			return m_RightBraceLoc;
		}

		std::size_t GetInitExprCount() const noexcept
		{
			return m_InitExprs.size();
		}

		NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> GetInitExprs() const noexcept;

		DEFAULT_ACCEPT_DECL;

	private:
		SourceLocation m_LeftBraceLoc, m_RightBraceLoc;
		std::vector<ExprPtr> m_InitExprs;
	};

	class BinaryOperator
		: public Expr
	{
	public:
		BinaryOperator(ExprPtr leftOperand, ExprPtr rightOperand, BinaryOperationType opcode, Type::TypePtr type, SourceLocation loc)
			: Expr{ BinaryOperatorClass, std::move(type), loc, loc }, m_LeftOperand{ std::move(leftOperand) }, m_RightOperand{ std::move(rightOperand) }, m_Opcode{ opcode }
		{
		}

		~BinaryOperator();

		ExprPtr GetLeftOperand() const noexcept
		{
			return m_LeftOperand;
		}

		void SetLeftOperand(ExprPtr value) noexcept
		{
			m_LeftOperand = std::move(value);
		}

		ExprPtr GetRightOperand() const noexcept
		{
			return m_RightOperand;
		}

		void SetRightOperand(ExprPtr value) noexcept
		{
			m_RightOperand = std::move(value);
		}

		BinaryOperationType GetOpcode() const noexcept
		{
			return m_Opcode;
		}

		void SetOpcode(BinaryOperationType value) noexcept
		{
			m_Opcode = value;
		}

		Statement::StmtEnumerable GetChildrenStmt() override;
		
		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_LeftOperand, m_RightOperand;
		BinaryOperationType m_Opcode;
	};

	class CompoundAssignOperator
		: public BinaryOperator
	{
	public:
		CompoundAssignOperator(ExprPtr leftOperand, ExprPtr rightOperand, BinaryOperationType opcode, Type::TypePtr type, SourceLocation loc)
			: BinaryOperator{ std::move(leftOperand), std::move(rightOperand), opcode, std::move(type), loc }
		{
		}

		~CompoundAssignOperator();

		DEFAULT_ACCEPT_DECL;
	};

	class ConditionalOperator
		: public Expr
	{
	public:
		ConditionalOperator(ExprPtr cond, SourceLocation qLoc, ExprPtr leftOperand, SourceLocation cLoc, ExprPtr rightOperand, Type::TypePtr type)
			: Expr{ ConditionalOperatorClass, std::move(type), cond->GetStartLoc(), rightOperand->GetEndLoc() }, m_QuesionLoc{ qLoc }, m_ColonLoc{ cLoc },
			m_Condition{ std::move(cond) }, m_LeftOperand{ std::move(leftOperand) }, m_RightOperand{ std::move(rightOperand) }
		{
		}

		~ConditionalOperator();

		SourceLocation GetQuesionLoc() const noexcept
		{
			return m_QuesionLoc;
		}

		void SetQuesionLoc(SourceLocation value) noexcept
		{
			m_QuesionLoc = value;
		}

		SourceLocation GetColonLoc() const noexcept
		{
			return m_ColonLoc;
		}

		void SetColonLoc(SourceLocation value) noexcept
		{
			m_ColonLoc = value;
		}

		ExprPtr GetCondition() const noexcept
		{
			return m_Condition;
		}

		void SetCondition(ExprPtr value) noexcept
		{
			m_Condition = std::move(value);
			SetStartLoc(m_Condition->GetStartLoc());
		}

		ExprPtr GetLeftOperand() const noexcept
		{
			return m_LeftOperand;
		}

		void SetLeftOperand(ExprPtr value) noexcept
		{
			m_LeftOperand = std::move(value);
		}

		ExprPtr GetRightOperand() const noexcept
		{
			return m_RightOperand;
		}

		void SetRightOperand(ExprPtr value) noexcept
		{
			m_RightOperand = std::move(value);
			SetEndLoc(m_RightOperand->GetEndLoc());
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		SourceLocation m_QuesionLoc, m_ColonLoc;
		ExprPtr m_Condition, m_LeftOperand, m_RightOperand;
	};

	class StmtExpr
		: public Expr
	{
	public:
		StmtExpr(NatsuLib::natRefPointer<Statement::CompoundStmt> subStmt, Type::TypePtr type, SourceLocation start, SourceLocation end)
			: Expr{ StmtExprClass, std::move(type), start, end }, m_SubStmt{ std::move(subStmt) }
		{
		}

		~StmtExpr();

		NatsuLib::natRefPointer<Statement::CompoundStmt> GetSubStmt() const noexcept
		{
			return m_SubStmt;
		}

		void SetSubStmt(NatsuLib::natRefPointer<Statement::CompoundStmt> value) noexcept
		{
			m_SubStmt = std::move(value);
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		NatsuLib::natRefPointer<Statement::CompoundStmt> m_SubStmt;
	};

	class ThisExpr
		: public Expr
	{
	public:
		ThisExpr(SourceLocation loc, Type::TypePtr type, nBool isImplicit)
			: Expr{ ThisExprClass, std::move(type), loc, loc }, m_IsImplicit{ isImplicit }
		{
		}

		~ThisExpr();

		nBool IsImplicit() const noexcept
		{
			return m_IsImplicit;
		}

		void SetImplicit(nBool value) noexcept
		{
			m_IsImplicit = value;
		}

		DEFAULT_ACCEPT_DECL;

	private:
		nBool m_IsImplicit;
	};

	class ThrowExpr
		: public Expr
	{
	public:
		ThrowExpr(ExprPtr operand, Type::TypePtr type, SourceLocation loc)
			: Expr{ ThrowExprClass, std::move(type), loc, operand ? operand->GetEndLoc() : loc }, m_Operand{ std::move(operand) }
		{
		}

		~ThrowExpr();

		ExprPtr GetOperand() const noexcept
		{
			return m_Operand;
		}

		void SetOperand(ExprPtr value) noexcept
		{
			m_Operand = std::move(value);
			SetEndLoc(m_Operand ? m_Operand->GetEndLoc() : GetStartLoc());
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_Operand;
	};

	class ConstructExpr
		: public Expr
	{
	public:
		ConstructExpr(Type::TypePtr type, SourceLocation loc, NatsuLib::natRefPointer<Declaration::ConstructorDecl> constructorDecl, std::vector<ExprPtr> args)
			: Expr{ ConstructExprClass, std::move(type), loc, loc }, m_ConstructorDecl{ std::move(constructorDecl) }, m_Args{ std::move(args) }
		{
		}

		~ConstructExpr();

		NatsuLib::natRefPointer<Declaration::ConstructorDecl> GetConstructorDecl() const noexcept
		{
			return m_ConstructorDecl;
		}

		void SetConstructorDecl(NatsuLib::natRefPointer<Declaration::ConstructorDecl> value) noexcept
		{
			m_ConstructorDecl = std::move(value);
		}

		NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> GetArgs() const noexcept;
		std::size_t GetArgCount() const noexcept;
		void SetArgs(NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> const& value);
		void SetArgs(std::vector<ExprPtr> value);

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		NatsuLib::natRefPointer<Declaration::ConstructorDecl> m_ConstructorDecl;
		std::vector<ExprPtr> m_Args;
	};

	class NewExpr
		: public Expr
	{
	public:
		NewExpr(NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> const& args, Type::TypePtr type, SourceRange range)
			: Expr{ NewExprClass, std::move(type), range.GetBegin(), range.GetEnd() }, m_Args{ args.begin(), args.end() }
		{
		}

		~NewExpr();

		NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> GetArgs() const noexcept;
		void SetArgs(NatsuLib::Linq<NatsuLib::Valued<ExprPtr>> const& value);

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		std::vector<ExprPtr> m_Args;
	};

	class DeleteExpr
		: public Expr
	{
	public:
		DeleteExpr(Type::TypePtr type, ExprPtr operand, SourceLocation loc)
			: Expr{ DeleteExprClass, std::move(type), loc, loc }, m_Operand{ std::move(operand) }
		{
		}

		~DeleteExpr();

		ExprPtr GetOperand() const noexcept
		{
			return m_Operand;
		}

		void SetOperand(ExprPtr value) noexcept
		{
			m_Operand = std::move(value);
		}

		Statement::StmtEnumerable GetChildrenStmt() override;

		DEFAULT_ACCEPT_DECL;

	private:
		ExprPtr m_Operand;
	};

	// TODO
	class OverloadExpr
		: public Expr
	{
	public:
		OverloadExpr(StmtType stmtType, Identifier::IdPtr name);
		~OverloadExpr();

		DEFAULT_ACCEPT_DECL;

	private:
		Identifier::IdPtr m_Name;
		std::vector<Declaration::DeclPtr> m_Decls;
	};
}

#undef DEFAULT_ACCEPT_DECL
