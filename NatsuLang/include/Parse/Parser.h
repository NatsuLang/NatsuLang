﻿#pragma once
#include "AST/OperatorPrecedence.h"
#include "Lex/Preprocessor.h"
#include "Basic/Config.h"

namespace NatsuLang
{
	class NestedNameSpecifier;
}

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
	using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
}

namespace NatsuLang::Declaration
{
	class Declarator;
	class Decl;
	using DeclPtr = NatsuLib::natRefPointer<Decl>;
	enum class Context;
}

namespace NatsuLang::Statement
{
	class Stmt;
	using StmtPtr = NatsuLib::natRefPointer<Stmt>;
}

namespace NatsuLang::Expression
{
	class Expr;
	using ExprPtr = NatsuLib::natRefPointer<Expr>;
}

namespace NatsuLang::Diag
{
	class DiagnosticsEngine;
}

namespace NatsuLang::Semantic
{
	enum class ScopeFlags : nuShort;
	class Sema;
}

namespace NatsuLang::Type
{
	class Type;
	using TypePtr = NatsuLib::natRefPointer<Type>;
}

namespace NatsuLang::Syntax
{
	DeclareException(ParserException, NatsuLib::natException, "Exception generated by parser.");

	class Parser
	{
		class ParseScope
			: NatsuLib::nonmovable
		{
		public:
			ParseScope(Parser* self, Semantic::ScopeFlags flags);
			~ParseScope();

			void ExplicitExit();

		private:
			Parser* m_Self;
		};

	public:
		Parser(Preprocessor& preprocessor, Semantic::Sema& sema);
		~Parser();

		Preprocessor& GetPreprocessor() const noexcept;
		Diag::DiagnosticsEngine& GetDiagnosticsEngine() const noexcept;
		Semantic::Sema& GetSema() const noexcept;

		void ConsumeToken()
		{
			m_Preprocessor.Lex(m_CurrentToken);
		}

		void ConsumeParen()
		{
			assert(IsParen(m_CurrentToken.GetType()));
			if (m_CurrentToken.Is(Lex::TokenType::LeftParen))
			{
				++m_ParenCount;
			}
			else if (m_ParenCount)
			{
				--m_ParenCount;
			}
			ConsumeToken();
		}

		void ConsumeBracket()
		{
			assert(IsBracket(m_CurrentToken.GetType()));
			if (m_CurrentToken.Is(Lex::TokenType::LeftSquare))
			{
				++m_BracketCount;
			}
			else if (m_BracketCount)
			{
				--m_BracketCount;
			}
			ConsumeToken();
		}

		void ConsumeBrace()
		{
			assert(IsBrace(m_CurrentToken.GetType()));
			if (m_CurrentToken.Is(Lex::TokenType::LeftBrace))
			{
				++m_BraceCount;
			}
			else if (m_BraceCount)
			{
				--m_BraceCount;
			}
			ConsumeToken();
		}

		void ConsumeAnyToken()
		{
			const auto type = m_CurrentToken.GetType();
			if (IsParen(type))
			{
				ConsumeParen();
			}
			else if (IsBracket(type))
			{
				ConsumeBracket();
			}
			else if (IsBrace(type))
			{
				ConsumeBrace();
			}
			else
			{
				ConsumeToken();
			}
		}

#if PARSER_USE_EXCEPTION
		[[noreturn]] static Expression::ExprPtr ParseExprError();
		[[noreturn]] static Statement::StmtPtr ParseStmtError();
		[[noreturn]] static Declaration::DeclPtr ParseDeclError();
#else
		static Expression::ExprPtr ParseExprError() noexcept;
		static Statement::StmtPtr ParseStmtError() noexcept;
		static Declaration::DeclPtr ParseDeclError() noexcept;
#endif

		///	@brief	分析顶层声明
		///	@param	decls	输出分析得到的顶层声明
		///	@return	是否遇到EOF
		nBool ParseTopLevelDecl(std::vector<Declaration::DeclPtr>& decls);
		std::vector<Declaration::DeclPtr> ParseExternalDeclaration();

		std::vector<Declaration::DeclPtr> ParseModuleImport();
		std::vector<Declaration::DeclPtr> ParseModuleDecl();
		nBool ParseModuleName(std::vector<std::pair<NatsuLib::natRefPointer<Identifier::IdentifierInfo>, SourceLocation>>& path);

		std::vector<Declaration::DeclPtr> ParseDeclaration(Declaration::Context context, SourceLocation& declEnd);

		Declaration::DeclPtr ParseFunctionBody(Declaration::DeclPtr decl, ParseScope& scope);

		Statement::StmtPtr ParseStatement();
		Statement::StmtPtr ParseLabeledStatement(Identifier::IdPtr labelId, SourceLocation labelLoc);
		Statement::StmtPtr ParseCompoundStatement();
		Statement::StmtPtr ParseCompoundStatement(Semantic::ScopeFlags flags);
		Statement::StmtPtr ParseIfStatement();
		Statement::StmtPtr ParseWhileStatement();

		Statement::StmtPtr ParseContinueStatement();
		Statement::StmtPtr ParseBreakStatement();
		Statement::StmtPtr ParseReturnStatement();

		Statement::StmtPtr ParseExprStatement();

		Expression::ExprPtr ParseExpression();

		// cast-expression:
		//	unary-expression
		//	cast-expression 'as' type-name
		// unary-expression:
		//	postfix-expression
		//	'++' unary-expression
		//	'--' unary-expression
		//	unary-operator cast-expression
		//	new-expression
		//	delete-expression
		// unary-operator: one of
		//	'+' '-' '!' '~'
		// primary-expression:
		//	id-expression
		//	literal
		//	this
		//	'(' expression ')'
		// id-expression:
		//	unqualified-id
		//	qualified-id
		// unqualified-id:
		//	identifier
		// new-expression:
		//	TODO
		// delete-expression:
		//	TODO
		Expression::ExprPtr ParseCastExpression();
		Expression::ExprPtr ParseAsTypeExpression(Expression::ExprPtr operand);

		Expression::ExprPtr ParseRightOperandOfBinaryExpression(Expression::ExprPtr leftOperand, OperatorPrecedence minPrec = OperatorPrecedence::Assignment);

		// postfix-expression:
		//	primary-expression
		//	postfix-expression '[' expression ']'
		//	postfix-expression '(' argument-expression-list[opt] ')'
		//	postfix-expression '.' identifier
		//	postfix-expression '++'
		//	postfix-expression '--'
		Expression::ExprPtr ParsePostfixExpressionSuffix(Expression::ExprPtr prefix);
		Expression::ExprPtr ParseConstantExpression();
		Expression::ExprPtr ParseAssignmentExpression();
		Expression::ExprPtr ParseThrowExpression();
		Expression::ExprPtr ParseParenExpression();

		// unqualified-id:
		//	identifier
		nBool ParseUnqualifiedId(Identifier::IdPtr& result);

		// argument-expression-list:
		//	argument-expression
		//	argument-expression-list ',' argument-expression
		nBool ParseExpressionList(std::vector<Expression::ExprPtr>& exprs, std::vector<SourceLocation>& commaLocs);

		void ParseDeclarator(Declaration::Declarator& decl);
		void ParseSpecifier(Declaration::Declarator& decl);

		void ParseType(Declaration::Declarator& decl);
		void ParseTypeOfType(Declaration::Declarator& decl);
		void ParseParenType(Declaration::Declarator& decl);
		void ParseFunctionType(Declaration::Declarator& decl);
		void ParseArrayType(Declaration::Declarator& decl);

		void ParseInitializer(Declaration::Declarator& decl);

		nBool SkipUntil(std::initializer_list<Lex::TokenType> list, nBool dontConsume = false);

	private:
		Preprocessor& m_Preprocessor;
		Diag::DiagnosticsEngine& m_Diag;
		Semantic::Sema& m_Sema;

		Lex::Token m_CurrentToken;
		nuInt m_ParenCount, m_BracketCount, m_BraceCount;
	};
}

namespace NatsuLang
{
	class ASTContext;
	struct ASTConsumer;

	void ParseAST(Preprocessor& pp, ASTContext& astContext, NatsuLib::natRefPointer<ASTConsumer> astConsumer);
	void ParseAST(Syntax::Parser& parser);
}
