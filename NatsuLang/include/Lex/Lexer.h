﻿#pragma once
#include "Basic/Token.h"

namespace NatsuLang
{
	class Preprocessor;

	namespace Lex
	{
		DeclareException(LexerException, NatsuLib::natException, "Exception generated by lexer.");

		class Lexer
			: public NatsuLib::natRefObjImpl<Lexer>, public NatsuLib::nonmovable
		{
		public:
			explicit Lexer(nStrView buffer, Preprocessor& preprocessor);

			nBool Lex(Lex::Token& result);

			nuInt GetFileID() const noexcept;
			void SetFileID(nuInt value) noexcept;

		private:
			using Iterator = nStrView::const_iterator;
			using CharType = nStrView::CharType;

			Preprocessor& m_Preprocessor;

			SourceLocation m_CurLoc;
			nStrView m_Buffer;
			// 当前处理的指针，指向下一次被处理的字符
			Iterator m_Current;

			nBool skipWhitespace(Lex::Token& result, Iterator cur);
			nBool skipLineComment(Lex::Token& result, Iterator cur);
			nBool skipBlockComment(Lex::Token& result, Iterator cur);

			nBool lexNumericLiteral(Lex::Token& result, Iterator cur);
			nBool lexIdentifier(Lex::Token& result, Iterator cur);
			nBool lexCharLiteral(Lex::Token& result, Iterator cur);
			nBool lexStringLiteral(Lex::Token& result, Iterator cur);
		};
	}
}
