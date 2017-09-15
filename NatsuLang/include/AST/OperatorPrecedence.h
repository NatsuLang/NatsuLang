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

	constexpr OperatorPrecedence GetOperatorPrecedence(Lex::TokenType tokenType)
	{
		switch (tokenType)
		{
		case Lex::TokenType::AmpEqual:
		case Lex::TokenType::PlusEqual:
		case Lex::TokenType::MinusEqual:
		case Lex::TokenType::SlashEqual:
		case Lex::TokenType::StarEqual:
		case Lex::TokenType::PercentEqual:
		case Lex::TokenType::LessLessEqual:
		case Lex::TokenType::GreaterGreaterEqual:
		case Lex::TokenType::CaretEqual:
		case Lex::TokenType::PipeEqual:
		case Lex::TokenType::Equal:
			return OperatorPrecedence::Assignment;
		case Lex::TokenType::Amp:
			return OperatorPrecedence::And;
		case Lex::TokenType::AmpAmp:
			return OperatorPrecedence::LogicalAnd;
		case Lex::TokenType::Star:
		case Lex::TokenType::Slash:
		case Lex::TokenType::Percent:
			return OperatorPrecedence::Multiplicative;
		case Lex::TokenType::Plus:
		case Lex::TokenType::Minus:
			return OperatorPrecedence::Additive;
		case Lex::TokenType::ExclaimEqual:
		case Lex::TokenType::EqualEqual:
			return OperatorPrecedence::Equality;
		case Lex::TokenType::Less:
		case Lex::TokenType::Greater:
		case Lex::TokenType::LessEqual:
		case Lex::TokenType::GreaterEqual:
			return OperatorPrecedence::Relational;
		case Lex::TokenType::LessLess:
		case Lex::TokenType::GreaterGreater:
			return OperatorPrecedence::Shift;
		case Lex::TokenType::Caret:
			return OperatorPrecedence::ExclusiveOr;
		case Lex::TokenType::Pipe:
			return OperatorPrecedence::InclusiveOr;
		case Lex::TokenType::PipePipe:
			return OperatorPrecedence::LogicalOr;
		case Lex::TokenType::Question:
			return OperatorPrecedence::Conditional;
		default:
			return OperatorPrecedence::Unknown;
		}
	}
}
