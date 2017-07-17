#include "Parse/Parser.h"
#include "Sema/Sema.h"
#include "Sema/Declarator.h"
#include "AST/Type.h"
#include "AST/ASTContext.h"

using namespace NatsuLib;
using namespace NatsuLang::Syntax;
using namespace NatsuLang::Token;

Parser::Parser(Preprocessor& preprocessor, Semantic::Sema& sema)
	: m_Preprocessor{ preprocessor }, m_DiagnosticsEngine{ preprocessor.GetDiag() }, m_Sema{ sema }, m_ParenCount{}, m_BracketCount{}, m_BraceCount{}
{
	m_CurrentToken.SetType(TokenType::Eof);
}

Parser::~Parser()
{
}

NatsuLang::Preprocessor& Parser::GetPreprocessor() const noexcept
{
	return m_Preprocessor;
}

NatsuLang::Diag::DiagnosticsEngine& Parser::GetDiagnosticsEngine() const noexcept
{
	return m_DiagnosticsEngine;
}

nBool Parser::ParseTopLevelDecl(std::vector<natRefPointer<Declaration::Decl>>& decls)
{
	if (m_CurrentToken.Is(TokenType::Eof))
	{
		ConsumeToken();
	}

	switch (m_CurrentToken.GetType())
	{
	case TokenType::Kw_import:
		decls = ParseModuleImport();
		return false;
	case TokenType::Kw_module:
		decls = ParseModuleDecl();
		return false;
	case TokenType::Eof:
		return true;
	default:
		break;
	}

	decls = ParseExternalDeclaration();
	return false;
}

std::vector<natRefPointer<NatsuLang::Declaration::Decl>> Parser::ParseExternalDeclaration()
{
	switch (m_CurrentToken.GetType())
	{
	case TokenType::Semi:
		// Empty Declaration
		ConsumeToken();
		break;
	case TokenType::RightBrace:
		m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExtraneousClosingBrace);
		ConsumeBrace();
		return {};
	case TokenType::Eof:
		m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrUnexpectEOF);
		return {};
	case TokenType::Kw_def:
		return ParseDeclaration();
	default:
		break;
	}
}

std::vector<natRefPointer<NatsuLang::Declaration::Decl>> Parser::ParseModuleImport()
{
	assert(m_CurrentToken.Is(Token::TokenType::Kw_import));
	ConsumeToken();
	auto startLoc = m_PrevTokenLocation;

	std::vector<std::pair<natRefPointer<Identifier::IdentifierInfo>, SourceLocation>> path;
	if (!ParseModuleName(path))
	{
		return {};
	}

	nat_Throw(NotImplementedException);
}

std::vector<natRefPointer<NatsuLang::Declaration::Decl>> Parser::ParseModuleDecl()
{
	nat_Throw(NotImplementedException);
}

nBool Parser::ParseModuleName(std::vector<std::pair<natRefPointer<Identifier::IdentifierInfo>, SourceLocation>>& path)
{
	while (true)
	{
		if (!m_CurrentToken.Is(TokenType::Identifier))
		{
			m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
			// SkipUntil({ TokenType::Semi });
			return false;
		}

		path.emplace_back(m_CurrentToken.GetIdentifierInfo(), m_CurrentToken.GetLocation());
		ConsumeToken();

		if (!m_CurrentToken.Is(TokenType::Period))
		{
			return true;
		}

		ConsumeToken();
	}
}

// declaration:
//	simple-declaration
// simple-declaration:
//	def declarator [;]
std::vector<natRefPointer<NatsuLang::Declaration::Decl>> Parser::ParseDeclaration()
{
	assert(m_CurrentToken.Is(TokenType::Kw_def));
	// 吃掉 def
	ConsumeToken();

	
}

NatsuLang::Expression::ExprPtr Parser::ParseExpression()
{
	// TODO
	nat_Throw(NotImplementedException);
}

NatsuLang::Expression::ExprPtr Parser::ParseConstantExpression()
{
	// TODO
	nat_Throw(NotImplementedException);
}

// declarator:
//	[identifier] [specifier-seq] [initializer] [;]
void Parser::ParseDeclarator(Declaration::Declarator& decl)
{
	const auto context = decl.GetContext();
	if (m_CurrentToken.Is(TokenType::Identifier))
	{
		decl.SetIdentifier(m_CurrentToken.GetIdentifierInfo());
		ConsumeToken();
	}
	else if (context != Declaration::Declarator::Context::Prototype)
	{
		m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
		return;
	}

	// (: int)也可以？
	if (m_CurrentToken.Is(TokenType::Colon) || (context == Declaration::Declarator::Context::Prototype && !decl.GetIdentifier()))
	{
		ParseSpecifier(decl);
	}

	// 声明函数原型时也可以指定initializer？
	if (m_CurrentToken.IsAnyOf({ TokenType::Equal, TokenType::LeftBrace }))
	{
		ParseInitializer(decl);
	}
}

// specifier-seq:
//	specifier-seq specifier
// specifier:
//	type-specifier
void Parser::ParseSpecifier(Declaration::Declarator& decl)
{
	ParseType(decl);
}

// type-specifier:
//	[auto]
//	typeof-specifier
//	type-identifier
void Parser::ParseType(Declaration::Declarator& decl)
{
	const auto context = decl.GetContext();
	const auto token = m_CurrentToken;
	ConsumeToken();

	if (!token.Is(TokenType::Colon) && context != Declaration::Declarator::Context::Prototype)
	{
		// 在非声明函数原型的上下文中不显式写出类型，视为隐含auto
		// auto的声明符在指定initializer之后决定实际类型
		return;
	}

	const auto tokenType = m_CurrentToken.GetType();
	switch (tokenType)
	{
	case TokenType::Identifier:
	{
		// 普通或数组类型

		auto type = m_Sema.GetTypeName(m_CurrentToken.GetIdentifierInfo(), m_CurrentToken.GetLocation(), m_Sema.GetCurrentScope(), nullptr);
		if (!type)
		{
			return;
		}

		ConsumeToken();

		break;
	}
	case TokenType::LeftParen:
	{
		// 函数类型或者括号类型
		ParseParenType(decl);

		break;
	}
	case TokenType::RightParen:
	{
		ConsumeParen();

		return;
	}
	case TokenType::Kw_typeof:
	{
		// typeof

		break;
	}
	default:
	{
		const auto builtinClass = Type::BuiltinType::GetBuiltinClassFromTokenType(tokenType);
		if (builtinClass == Type::BuiltinType::Invalid)
		{
			// 对于无效类型的处理
			m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedTypeSpecifierGot, m_CurrentToken.GetLocation())
				.AddArgument(tokenType);
			return;
		}
		decl.SetType(m_Sema.GetASTContext().GetBuiltinType(builtinClass));
		break;
	}
	}

	// 即使类型不是数组尝试Parse也不会错误
	ParseArrayType(decl);
}

void Parser::ParseParenType(Declaration::Declarator& decl)
{
	assert(m_CurrentToken.Is(TokenType::LeftParen));
	// 吃掉左括号
	ConsumeParen();

	auto innerType = ParseType();
	if (!innerType)
	{
		if (m_CurrentToken.Is(TokenType::Identifier))
		{
			// 函数类型
			return ParseFunctionType();
		}
		
		m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier);
	}

	return innerType;
}

void Parser::ParseFunctionType(Declaration::Declarator& decl)
{
	assert(m_CurrentToken.Is(TokenType::Identifier));

	std::vector<Type::TypePtr> paramTypes;
	std::vector<Identifier::IdPtr> paramNames;

	while (true)
	{
		Declaration::Declarator param{ Declaration::Declarator::Context::Prototype };
		ParseDeclarator(param);
		ConsumeToken();
		if (m_CurrentToken.Is(TokenType::Identifier))
		{
			paramNames.emplace_back(m_CurrentToken.GetIdentifierInfo());
		}

		ConsumeToken();

		if (m_CurrentToken.Is(TokenType::RightParen))
		{
			ConsumeParen();
			break;
		}

		if (!m_CurrentToken.Is(TokenType::Comma))
		{
			m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				.AddArgument(TokenType::Comma)
				.AddArgument(m_CurrentToken.GetType());
		}

		ConsumeToken();
	}

	// 读取完函数参数信息，开始读取返回类型

	if (!m_CurrentToken.Is(TokenType::Arrow))
	{
		m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::Arrow)
			.AddArgument(m_CurrentToken.GetType());
	}

	ConsumeToken();

	auto retType = ParseType();
}

void Parser::ParseArrayType(Declaration::Declarator& decl)
{
	while (m_CurrentToken.Is(TokenType::LeftSquare))
	{
		ConsumeBracket();
		auto countExpr = ParseConstantExpression();
		decl.SetType(m_Sema.GetASTContext().GetArrayType(decl.GetType(), countExpr));
	}
}

// initializer:
//	= expression
//	{ expression }
//	{ statement }
void Parser::ParseInitializer(Declaration::Declarator& decl)
{

}

nBool Parser::SkipUntil(std::initializer_list<Token::TokenType> list, nBool dontConsume)
{
	// 特例，如果调用者只是想跳到文件结尾，我们不需要再另外判断其他信息
	if (list.size() == 1 && *list.begin() == TokenType::Eof)
	{
		while (!m_CurrentToken.Is(TokenType::Eof))
		{
			ConsumeAnyToken();
		}
		return true;
	}

	while (true)
	{
		const auto currentType = m_CurrentToken.GetType();
		for (auto type : list)
		{
			if (currentType == type)
			{
				if (!dontConsume)
				{
					ConsumeToken();
				}

				return true;
			}
		}

		if (currentType == TokenType::Eof)
		{
			return false;
		}

		ConsumeAnyToken();
	}
}
