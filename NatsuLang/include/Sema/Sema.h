#pragma once
#include <natMisc.h>
#include <natRefObj.h>
#include <unordered_set>
#include "Basic/SourceLocation.h"
#include "Basic/Identifier.h"
#include "AST/Declaration.h"
#include "AST/Type.h"
#include "AST/OperationTypes.h"

namespace NatsuLang
{
	namespace Expression
	{
		enum class UnaryOperationType;
	}

	namespace Declaration
	{
		class Declarator;
	}

	namespace Diag
	{
		class DiagnosticsEngine;
	}

	namespace Identifier
	{
		class IdentifierInfo;
		using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
	}

	class Preprocessor;
	class ASTContext;
	class NestedNameSpecifier;
	class SourceManager;
}

namespace NatsuLang::Semantic
{
	class LookupResult;
	class Scope;

	class Sema
		: public NatsuLib::nonmovable
	{
	public:
		enum class ExpressionEvaluationContext
		{
			Unevaluated,
			DiscardedStatement,
			ConstantEvaluated,
			PotentiallyEvaluated,
			PotentiallyEvaluatedIfUsed
		};

		enum class LookupNameType
		{
			LookupOrdinaryName,
			LookupTagName,
			LookupLabel,
			LookupMemberName,
			LookupModuleName,
			LookupAnyName
		};

		using ModulePathType = std::vector<std::pair<NatsuLib::natRefPointer<Identifier::IdentifierInfo>, SourceLocation>>;

		explicit Sema(Preprocessor& preprocessor, ASTContext& astContext);
		~Sema();

		Preprocessor& GetPreprocessor() const noexcept
		{
			return m_Preprocessor;
		}

		ASTContext& GetASTContext() const noexcept
		{
			return m_Context;
		}

		Diag::DiagnosticsEngine& GetDiagnosticsEngine() const noexcept
		{
			return m_Diag;
		}

		SourceManager& GetSourceManager() const noexcept
		{
			return m_SourceManager;
		}

		NatsuLib::natRefPointer<Scope> GetCurrentScope() const noexcept
		{
			return m_CurrentScope;
		}

		void PushDeclContext(NatsuLib::natRefPointer<Scope> const& scope, Declaration::DeclContext* dc);
		void PopDeclContext();

		NatsuLib::natRefPointer<Declaration::Decl> OnModuleImport(SourceLocation startLoc, SourceLocation importLoc, ModulePathType const& path);

		Type::TypePtr GetTypeName(NatsuLib::natRefPointer<Identifier::IdentifierInfo> const& id, SourceLocation nameLoc, NatsuLib::natRefPointer<Scope> scope, Type::TypePtr const& objectType);

		nBool LookupName(LookupResult& result, NatsuLib::natRefPointer<Scope> scope) const;
		nBool LookupQualifiedName(LookupResult& result, Declaration::DeclContext* context) const;
		nBool LookupNestedName(LookupResult& result, NatsuLib::natRefPointer<Scope> scope, NatsuLib::natRefPointer<NestedNameSpecifier> const& nns);

		Type::TypePtr ActOnTypeName(NatsuLib::natRefPointer<Scope> const& scope, Declaration::Declarator const& decl);
		NatsuLib::natRefPointer<Declaration::ParmVarDecl> ActOnParamDeclarator(NatsuLib::natRefPointer<Scope> const& scope, Declaration::Declarator const& decl);
		NatsuLib::natRefPointer<Declaration::NamedDecl> HandleDeclarator(NatsuLib::natRefPointer<Scope> const& scope, Declaration::Declarator const& decl);

		Expression::ExprPtr ActOnBooleanLiteral(Token::Token const& token) const;
		Expression::ExprPtr ActOnNumericLiteral(Token::Token const& token) const;
		Expression::ExprPtr ActOnCharLiteral(Token::Token const& token) const;
		Expression::ExprPtr ActOnStringLiteral(Token::Token const& token);

		Expression::ExprPtr ActOnThrow(NatsuLib::natRefPointer<Scope> const& scope, SourceLocation loc, Expression::ExprPtr expr);

		Expression::ExprPtr ActOnIdExpr(NatsuLib::natRefPointer<Scope> const& scope, NatsuLib::natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, nBool hasTraillingLParen);
		Expression::ExprPtr ActOnThis(SourceLocation loc);
		Expression::ExprPtr ActOnAsTypeExpr(NatsuLib::natRefPointer<Scope> const& scope, Expression::ExprPtr exprToCast, Type::TypePtr type, SourceLocation loc);
		Expression::ExprPtr ActOnArraySubscriptExpr(NatsuLib::natRefPointer<Scope> const& scope, Expression::ExprPtr base, SourceLocation lloc, Expression::ExprPtr index, SourceLocation rloc);
		Expression::ExprPtr ActOnCallExpr(NatsuLib::natRefPointer<Scope> const& scope, Expression::ExprPtr func, SourceLocation lloc, NatsuLib::Linq<const Expression::ExprPtr> argExprs, SourceLocation rloc);
		Expression::ExprPtr ActOnMemberAccessExpr(NatsuLib::natRefPointer<Scope> const& scope, Expression::ExprPtr base, SourceLocation periodLoc, NatsuLib::natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id);
		Expression::ExprPtr ActOnUnaryOp(NatsuLib::natRefPointer<Scope> const& scope, SourceLocation loc, Token::TokenType tokenType, Expression::ExprPtr operand);
		Expression::ExprPtr ActOnPostfixUnaryOp(NatsuLib::natRefPointer<Scope> const& scope, SourceLocation loc, Token::TokenType tokenType, Expression::ExprPtr operand);
		Expression::ExprPtr ActOnBinaryOp(NatsuLib::natRefPointer<Scope> const& scope, SourceLocation loc, Token::TokenType tokenType, Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand);
		Expression::ExprPtr BuildBuiltinBinaryOp(SourceLocation loc, Expression::BinaryOperationType binOpType, Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand);
		// 条件操作符不可重载所以不需要scope信息
		Expression::ExprPtr ActOnConditionalOp(SourceLocation questionLoc, SourceLocation colonLoc, Expression::ExprPtr condExpr, Expression::ExprPtr leftExpr, Expression::ExprPtr rightExpr);

		Expression::ExprPtr BuildDeclarationNameExpr(NatsuLib::natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, NatsuLib::natRefPointer<Declaration::NamedDecl> decl);
		Expression::ExprPtr BuildDeclRefExpr(NatsuLib::natRefPointer<Declaration::ValueDecl> decl, Type::TypePtr type, Identifier::IdPtr id, NatsuLib::natRefPointer<NestedNameSpecifier> const& nns);
		Expression::ExprPtr BuildMemberReferenceExpr(NatsuLib::natRefPointer<Scope> const& scope, Expression::ExprPtr baseExpr, Type::TypePtr baseType, SourceLocation opLoc, NatsuLib::natRefPointer<NestedNameSpecifier> const& nns, LookupResult& r);
		Expression::ExprPtr BuildFieldReferenceExpr(Expression::ExprPtr baseExpr, SourceLocation opLoc, NatsuLib::natRefPointer<NestedNameSpecifier> const& nns, NatsuLib::natRefPointer<Declaration::FieldDecl> field, Identifier::IdPtr id);

		Expression::ExprPtr CreateBuiltinUnaryOp(SourceLocation opLoc, Expression::UnaryOperationType opCode, Expression::ExprPtr operand);

		Type::TypePtr UsualArithmeticConversions(Expression::ExprPtr& leftOperand, Expression::ExprPtr& rightOperand);

		Expression::ExprPtr ImpCastExprToType(Expression::ExprPtr expr, Type::TypePtr type, Expression::CastType castType);

	private:
		Preprocessor& m_Preprocessor;
		ASTContext& m_Context;
		Diag::DiagnosticsEngine& m_Diag;
		SourceManager& m_SourceManager;

		NatsuLib::natRefPointer<Scope> m_CurrentScope;
		// m_CurrentDeclContext必须为nullptr或者可以转换到DeclContext*，不保存DeclContext*是为了保留对Decl的强引用
		Declaration::DeclPtr m_CurrentDeclContext;

		Expression::CastType getCastType(Expression::ExprPtr operand, Type::TypePtr toType);

		Type::TypePtr handleFloatConversion(Expression::ExprPtr& leftOperand, Type::TypePtr leftOperandType, Expression::ExprPtr& rightOperand, Type::TypePtr rightOperandType);
	};

	class LookupResult
	{
	public:
		enum class LookupResultType
		{
			NotFound,
			Found,
			FoundOverloaded,
			Ambiguous
		};

		enum class AmbiguousType
		{
			// TODO
		};

		LookupResult(Sema& sema, Identifier::IdPtr id, SourceLocation loc, Sema::LookupNameType lookupNameType);

		Identifier::IdPtr GetLookupId() const noexcept
		{
			return m_LookupId;
		}

		Sema::LookupNameType GetLookupType() const noexcept
		{
			return m_LookupNameType;
		}

		NatsuLib::Linq<NatsuLib::natRefPointer<const Declaration::NamedDecl>> GetDecls() const noexcept;

		std::size_t GetDeclSize() const noexcept
		{
			return m_Decls.size();
		}

		nBool IsEmpty() const noexcept
		{
			return m_Decls.empty();
		}

		void AddDecl(NatsuLib::natRefPointer<Declaration::NamedDecl> decl);
		void AddDecl(NatsuLib::Linq<NatsuLib::natRefPointer<Declaration::NamedDecl>> decls);

		void ResolveResultType() noexcept;

		LookupResultType GetResultType() const noexcept
		{
			return m_Result;
		}

		AmbiguousType GetAmbiguousType() const noexcept
		{
			return m_AmbiguousType;
		}

		Type::TypePtr GetBaseObjectType() const noexcept
		{
			return m_BaseObjectType;
		}

		void SetBaseObjectType(Type::TypePtr value) noexcept
		{
			m_BaseObjectType = std::move(value);
		}

	private:
		// 查找参数
		Sema& m_Sema;
		Identifier::IdPtr m_LookupId;
		SourceLocation m_LookupLoc;
		Sema::LookupNameType m_LookupNameType;
		Declaration::IdentifierNamespace m_IDNS;

		// 查找结果
		LookupResultType m_Result;
		AmbiguousType m_AmbiguousType;
		std::unordered_set<NatsuLib::natRefPointer<Declaration::NamedDecl>> m_Decls;
		Type::TypePtr m_BaseObjectType;
	};
}
