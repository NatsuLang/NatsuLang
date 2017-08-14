#pragma once
#include "Basic/Token.h"

namespace NatsuLang
{
	enum class OperatorPrecedence
	{
		Unknown,
		Assignment,
		Conditional,
		LogicalOr,
		LogicalAnd,
		InclusiveOr,
		ExclusiveOr,
		And,
		Equality,
		Relational,
		Shift,
		Additive,
		Multiplicative,
	};

	constexpr OperatorPrecedence GetOperatorPrecedence(Token::TokenType tokenType)
	{
		switch (tokenType)
		{
		case Token::TokenType::AmpEqual:
		case Token::TokenType::PlusEqual:
		case Token::TokenType::MinusEqual:
		case Token::TokenType::SlashEqual:
		case Token::TokenType::StarEqual:
		case Token::TokenType::PercentEqual:
		case Token::TokenType::LessLessEqual:
		case Token::TokenType::GreaterGreaterEqual:
		case Token::TokenType::CaretEqual:
		case Token::TokenType::PipeEqual:
		case Token::TokenType::Equal:
			return OperatorPrecedence::Assignment;
		case Token::TokenType::Amp:
			return OperatorPrecedence::And;
		case Token::TokenType::AmpAmp:
			return OperatorPrecedence::LogicalAnd;
		case Token::TokenType::Star:
		case Token::TokenType::Slash:
		case Token::TokenType::Percent:
			return OperatorPrecedence::Multiplicative;
		case Token::TokenType::Plus:
		case Token::TokenType::Minus:
			return OperatorPrecedence::Additive;
		case Token::TokenType::ExclaimEqual:
		case Token::TokenType::EqualEqual:
			return OperatorPrecedence::Equality;
		case Token::TokenType::Less:
		case Token::TokenType::Greater:
		case Token::TokenType::LessEqual:
		case Token::TokenType::GreaterEqual:
			return OperatorPrecedence::Relational;
		case Token::TokenType::LessLess:
		case Token::TokenType::GreaterGreater:
			return OperatorPrecedence::Shift;
		case Token::TokenType::Caret:
			return OperatorPrecedence::ExclusiveOr;
		case Token::TokenType::Pipe:
			return OperatorPrecedence::InclusiveOr;
		case Token::TokenType::PipePipe:
			return OperatorPrecedence::LogicalOr;
		case Token::TokenType::Question:
			return OperatorPrecedence::Conditional;
		default:
			return OperatorPrecedence::Unknown;
		}
	}
}
