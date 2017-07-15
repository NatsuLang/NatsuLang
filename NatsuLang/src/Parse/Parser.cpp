#include "Parse/Parser.h"
#include "Sema/Sema.h"
#include "AST/Type.h"

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

std::vector<natRefPointer<NatsuLang::Declaration::Decl>> Parser::ParseDeclaration()
{
	assert(m_CurrentToken.Is(TokenType::Kw_def));
	// 吃掉 def
	ConsumeToken();

	if (!m_CurrentToken.Is(TokenType::Identifier))
	{
		m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
		return {};
	}

	Token::Token name = m_CurrentToken, type, initializer;

	ConsumeToken();

	if (m_CurrentToken.Is(TokenType::Colon))
	{
		ConsumeToken();
		if (!type.Is(TokenType::Identifier))
		{
			m_DiagnosticsEngine.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, m_CurrentToken.GetLocation());
			return {};
		}
		type = m_CurrentToken;
	}

	
}

void Parser::ParseDeclarator(Declaration::Declarator& decl)
{

}

NatsuLang::Type::TypePtr Parser::ParseType()
{
	Type::TypePtr result;
	if (m_CurrentToken.Is(TokenType::Identifier))
	{
		// 普通或数组类型

		auto type = m_Sema.GetTypeName(m_CurrentToken.GetIdentifierInfo(), m_CurrentToken.GetLocation(), m_Sema.GetCurrentScope(), nullptr);
		if (!type)
		{
			return nullptr;
		}

		ConsumeToken();

		// 数组类型
		if (m_CurrentToken.Is(TokenType::LeftSquare))
		{
			result = make_ref<Type::ArrayType>(type, 0);
		}
		else
		{

		}
	}
	else if (m_CurrentToken.Is(TokenType::LeftParen))
	{
		ConsumeParen();

		// 函数类型或者括号类型
		std::vector<Type::TypePtr> paramTypes;
		std::vector<Identifier::IdPtr> paramNames;

		while (true)
		{
			paramTypes.emplace_back(ParseType());
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
	else if (m_CurrentToken.Is(TokenType::Kw_typeof))
	{
		// typeof
	}
	else
	{
		return nullptr;
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
