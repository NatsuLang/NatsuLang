#pragma once
#include <unordered_set>
#include "Lex/Preprocessor.h"

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
	class Sema;
}

namespace NatsuLang::Type
{
	class Type;
	using TypePtr = NatsuLib::natRefPointer<Type>;
}

namespace NatsuLang::Syntax
{
	class Parser
	{
	public:
		Parser(Preprocessor& preprocessor, Semantic::Sema& sema);
		~Parser();

		Preprocessor& GetPreprocessor() const noexcept;
		Diag::DiagnosticsEngine& GetDiagnosticsEngine() const noexcept;

		void ConsumeToken()
		{
			m_PrevTokenLocation = m_CurrentToken.GetLocation();
			m_Preprocessor.Lex(m_CurrentToken);
		}

		void ConsumeParen()
		{
			assert(IsParen(m_CurrentToken.GetType()));
			if (m_CurrentToken.Is(Token::TokenType::LeftParen))
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
			if (m_CurrentToken.Is(Token::TokenType::LeftSquare))
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
			if (m_CurrentToken.Is(Token::TokenType::LeftBrace))
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

		///	@brief	分析顶层声明
		///	@param	decls	输出分析得到的顶层声明
		///	@return	是否遇到EOF
		nBool ParseTopLevelDecl(std::vector<NatsuLib::natRefPointer<Declaration::Decl>>& decls);
		std::vector<NatsuLib::natRefPointer<Declaration::Decl>> ParseExternalDeclaration();

		std::vector<NatsuLib::natRefPointer<Declaration::Decl>> ParseModuleImport();
		std::vector<NatsuLib::natRefPointer<Declaration::Decl>> ParseModuleDecl();
		nBool ParseModuleName(std::vector<std::pair<NatsuLib::natRefPointer<Identifier::IdentifierInfo>, SourceLocation>>& path);

		std::vector<NatsuLib::natRefPointer<Declaration::Decl>> ParseDeclaration(Declaration::Context context);

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
		Expression::ExprPtr ParseConstantExpression();
		Expression::ExprPtr ParseAssignmentExpression();
		Expression::ExprPtr ParseThrowExpression();
		Expression::ExprPtr ParseParenExpression();

		void ParseDeclarator(Declaration::Declarator& decl);
		void ParseSpecifier(Declaration::Declarator& decl);

		void ParseType(Declaration::Declarator& decl);
		void ParseParenType(Declaration::Declarator& decl);
		void ParseFunctionType(Declaration::Declarator& decl);
		void ParseArrayType(Declaration::Declarator& decl);

		void ParseInitializer(Declaration::Declarator& decl);

		nBool SkipUntil(std::initializer_list<Token::TokenType> list, nBool dontConsume = false);

	private:
		Preprocessor& m_Preprocessor;
		Diag::DiagnosticsEngine& m_DiagnosticsEngine;
		Semantic::Sema& m_Sema;

		Token::Token m_CurrentToken;
		SourceLocation m_PrevTokenLocation;
		nuInt m_ParenCount, m_BracketCount, m_BraceCount;
	};
}
