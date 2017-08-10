#include "Parse/Parser.h"
#include "Sema/Sema.h"
#include "Sema/Declarator.h"
#include "AST/Type.h"
#include "AST/ASTContext.h"

using namespace NatsuLib;
using namespace NatsuLang::Syntax;
using namespace NatsuLang::Token;
using namespace NatsuLang::Diag;

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

DiagnosticsEngine& Parser::GetDiagnosticsEngine() const noexcept
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
		m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExtraneousClosingBrace);
		ConsumeBrace();
		return {};
	case TokenType::Eof:
		m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrUnexpectEOF);
		return {};
	case TokenType::Kw_def:
		return ParseDeclaration(Declaration::Context::Global);
	default:
		break;
	}
}

std::vector<natRefPointer<NatsuLang::Declaration::Decl>> Parser::ParseModuleImport()
{
	assert(m_CurrentToken.Is(Token::TokenType::Kw_import));
	auto startLoc = m_CurrentToken.GetLocation();
	ConsumeToken();

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
			m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
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
std::vector<natRefPointer<NatsuLang::Declaration::Decl>> Parser::ParseDeclaration(Declaration::Context context)
{
	assert(m_CurrentToken.Is(TokenType::Kw_def));
	// 吃掉 def
	ConsumeToken();

	Declaration::Declarator decl{ context };
	ParseDeclarator(decl);

	return { m_Sema.HandleDeclarator(m_Sema.GetCurrentScope(), decl) };
}

NatsuLang::Expression::ExprPtr Parser::ParseExpression()
{
	// TODO
	nat_Throw(NotImplementedException);
}

NatsuLang::Expression::ExprPtr Parser::ParseCastExpression()
{
	Expression::ExprPtr result;
	const auto tokenType = m_CurrentToken.GetType();

	switch (tokenType)
	{
	case TokenType::LeftParen:
		result = ParseParenExpression();
		break;
	case TokenType::NumericLiteral:
		result = m_Sema.ActOnNumericLiteral(m_CurrentToken);
		ConsumeToken();
		break;
	case TokenType::CharLiteral:
		result = m_Sema.ActOnCharLiteral(m_CurrentToken);
		ConsumeToken();
		break;
	case TokenType::StringLiteral:
		// TODO
		break;
	case TokenType::Kw_true:
	case TokenType::Kw_false:
		result = m_Sema.ActOnBooleanLiteral(m_CurrentToken);
		ConsumeToken();
		break;
	case TokenType::Identifier:
	{
		auto id = m_CurrentToken.GetIdentifierInfo();
		ConsumeToken();
		result = m_Sema.ActOnIdExpression(m_Sema.GetCurrentScope(), nullptr, id, m_CurrentToken.Is(TokenType::LeftParen));
		break;
	}
	case TokenType::PlusPlus:
	case TokenType::MinusMinus:
	{
		// TODO
		break;
	}
	case TokenType::Plus:
	case TokenType::Minus:
	case TokenType::Exclaim:
	case TokenType::Tilde:
		// TODO
		break;
	case TokenType::Kw_this:
		break;
	default:
		return nullptr;
	}

	return ParsePostfixExpressionSuffix(std::move(result));
}

NatsuLang::Expression::ExprPtr Parser::ParsePostfixExpressionSuffix(Expression::ExprPtr prefix)
{
	Expression::ExprPtr result;
	
	while (true)
	{
		switch (m_CurrentToken.GetType())
		{
		case TokenType::LeftSquare:
		{
			auto lloc = m_CurrentToken.GetLocation();
			ConsumeBracket();
			auto index = ParseExpression();

			if (!m_CurrentToken.Is(TokenType::RightSquare))
			{
				m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
					.AddArgument(TokenType::RightSquare)
					.AddArgument(m_CurrentToken.GetType());

				return nullptr;
			}

			auto rloc = m_CurrentToken.GetLocation();
			prefix = m_Sema.ActOnArraySubscriptExpression(m_Sema.GetCurrentScope(), std::move(prefix), lloc, std::move(index), rloc);
			ConsumeBracket();

			break;
		}
		case TokenType::LeftParen:
		{

			break;
		}
		case TokenType::Period:
			break;
		case TokenType::PlusPlus:
			break;
		case TokenType::MinusMinus:
			break;
		default:
			return std::move(prefix);
		}
	}
}

NatsuLang::Expression::ExprPtr Parser::ParseConstantExpression()
{
	// TODO
	nat_Throw(NotImplementedException);
}

NatsuLang::Expression::ExprPtr Parser::ParseAssignmentExpression()
{
	if (m_CurrentToken.Is(TokenType::Kw_throw))
	{
		return ParseThrowExpression();
	}


}

NatsuLang::Expression::ExprPtr Parser::ParseThrowExpression()
{
	assert(m_CurrentToken.Is(TokenType::Kw_throw));
	auto throwLocation = m_CurrentToken.GetLocation();
	ConsumeToken();

	switch (m_CurrentToken.GetType())
	{
	case TokenType::Semi:
	case TokenType::RightParen:
	case TokenType::RightSquare:
	case TokenType::RightBrace:
	case TokenType::Colon:
	case TokenType::Comma:
		return {};
	default:
		auto expr = ParseAssignmentExpression();
		if (!expr)
		{
			return nullptr;
		}
		break;
	}
}

NatsuLang::Expression::ExprPtr Parser::ParseParenExpression()
{
	assert(m_CurrentToken.Is(TokenType::LeftParen));

	ConsumeParen();
	auto ret = ParseExpression();
	if (!m_CurrentToken.Is(TokenType::RightParen))
	{
		m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::RightParen)
			.AddArgument(m_CurrentToken.GetType());
	}
	else
	{
		ConsumeParen();
	}
	
	return ret;
}

nBool Parser::ParseExpressionList(std::vector<Expression::ExprPtr>& exprs, std::vector<SourceLocation>& commaLocs)
{

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
	else if (context != Declaration::Context::Prototype)
	{
		m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
		return;
	}

	// (: int)也可以？
	if (m_CurrentToken.Is(TokenType::Colon) || (context == Declaration::Context::Prototype && !decl.GetIdentifier()))
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

	if (!token.Is(TokenType::Colon) && context != Declaration::Context::Prototype)
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
		ParseFunctionType(decl);

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
			m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedTypeSpecifierGot, m_CurrentToken.GetLocation())
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

	ParseType(decl);
	if (!decl.GetType())
	{
		if (m_CurrentToken.Is(TokenType::Identifier))
		{
			// 函数类型
			ParseFunctionType(decl);
			return;
		}
		
		m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedIdentifier);
	}
}

void Parser::ParseFunctionType(Declaration::Declarator& decl)
{
	assert(m_CurrentToken.Is(TokenType::Identifier));

	std::vector<Declaration::Declarator> paramDecls;

	auto mayBeParenType = true;

	while (true)
	{
		Declaration::Declarator param{ Declaration::Context::Prototype };
		ParseDeclarator(param);
		if (mayBeParenType && param.GetIdentifier() || !param.GetType())
		{
			mayBeParenType = false;
		}

		if (param.IsValid())
		{
			paramDecls.emplace_back(std::move(param));
		}
		else
		{
			m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedDeclarator, m_CurrentToken.GetLocation());
		}

		if (!param.GetType() && !param.GetInitializer())
		{
			// 参数的类型和初始化器至少要存在一个
		}

		if (m_CurrentToken.Is(TokenType::RightParen))
		{
			ConsumeParen();
			break;
		}

		if (!m_CurrentToken.Is(TokenType::Comma))
		{
			m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				.AddArgument(TokenType::Comma)
				.AddArgument(m_CurrentToken.GetType());
		}

		mayBeParenType = false;
		ConsumeToken();
	}

	// 读取完函数参数信息，开始读取返回类型

	// 如果不是->且只有一个无名称参数说明是普通的括号类型
	if (!m_CurrentToken.Is(TokenType::Arrow))
	{
		if (mayBeParenType)
		{
			// 是括号类型，但是我们已经把Token处理完毕了。。。
			decl = std::move(paramDecls[0]);
			return;
		}

		// 以后会加入元组或匿名类型的支持吗？
		m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
			.AddArgument(TokenType::Arrow)
			.AddArgument(m_CurrentToken.GetType());
	}

	ConsumeToken();

	Declaration::Declarator retType{ Declaration::Context::Prototype };
	ParseType(retType);

	// 该交给Sema处理了，插入一个function prototype
}

void Parser::ParseArrayType(Declaration::Declarator& decl)
{
	while (m_CurrentToken.Is(TokenType::LeftSquare))
	{
		ConsumeBracket();
		auto countExpr = ParseConstantExpression();
		decl.SetType(m_Sema.GetASTContext().GetArrayType(decl.GetType(), countExpr));
		if (!m_CurrentToken.Is(TokenType::RightSquare))
		{
			m_DiagnosticsEngine.Report(DiagnosticsEngine::DiagID::ErrExpectedGot, m_CurrentToken.GetLocation())
				.AddArgument(TokenType::RightSquare)
				.AddArgument(m_CurrentToken.GetType());
		}
		ConsumeAnyToken();
	}
}

// initializer:
//	= expression
//	{ statement }
void Parser::ParseInitializer(Declaration::Declarator& decl)
{
	if (m_CurrentToken.Is(TokenType::Equal))
	{
		decl.SetInitializer(ParseExpression());
		return;
	}

	if (m_CurrentToken.Is(TokenType::LeftBrace))
	{
		if (decl.GetType()->GetType() != Type::Type::Function)
		{
			
		}
	}
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
