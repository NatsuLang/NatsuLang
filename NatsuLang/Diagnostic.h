#pragma once
#include <vector>
#include <natString.h>
#include "Token.h"

namespace NatsuLang::Diag
{
	class DiagnosticsEngine
	{
	public:
		enum class Level
		{
			Ignored,
			Note,
			Remark,
			Warning,
			Error,
			Fatal
		};

		enum class ArgumentType
		{
			String,
			SInt,
			UInt,
			TokenType,
			IdentifierInfo
		};

		DiagnosticsEngine();
		~DiagnosticsEngine();

		nuInt AddArgument(const nString* string);
		nuInt AddArgument(nInt sInt);
		nuInt AddArgument(nuInt uInt);
		nuInt AddArgument(Token::TokenType tokenType);
		nuInt AddArgument(const Identifier::IdentifierInfo* identifierInfo);

	private:
		union Argument
		{
			const nString* String;
			nInt SInt;
			nuInt UInt;
			Token::TokenType TokenType;
			const Identifier::IdentifierInfo* IdentifierInfo;
		};

		std::vector<std::pair<ArgumentType, Argument>> m_Arguments;

		nString convertArgumentToString(nuInt index);
	};

	class DiagnosticBuilder
	{
	public:
		DiagnosticBuilder();
		~DiagnosticBuilder();

	private:

	};
}
