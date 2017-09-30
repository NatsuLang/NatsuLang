#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Detail;

Interpreter::Interpreter(natRefPointer<TextReader<StringType::Utf8>> const& diagIdMapFile, natLog& logger)
	: m_DiagConsumer{ make_ref<InterpreterDiagConsumer>(*this) },
	  m_Diag{ make_ref<InterpreterDiagIdMap>(diagIdMapFile), m_DiagConsumer },
	  m_Logger{ logger },
	  m_SourceManager{ m_Diag, m_FileManager },
	  m_Preprocessor{ m_Diag, m_SourceManager },
	  m_Consumer{ make_ref<InterpreterASTConsumer>(*this) },
	  m_Sema{ m_Preprocessor, m_AstContext, m_Consumer },
	  m_Parser{ m_Preprocessor, m_Sema },
	  m_Visitor{ make_ref<InterpreterStmtVisitor>(*this) }, m_DeclStorage{ *this }
{
}

Interpreter::~Interpreter()
{
}

void Interpreter::Run(Uri const& uri)
{
	m_Preprocessor.SetLexer(make_ref<Lex::Lexer>(m_SourceManager.GetFileContent(m_SourceManager.GetFileID(uri)).second, m_Preprocessor));
	m_Parser.ConsumeToken();
	m_CurrentScope = m_Sema.GetCurrentScope();
	ParseAST(m_Parser);
	m_DeclStorage.GarbageCollect();
}

void Interpreter::Run(nStrView content)
{
	m_Preprocessor.SetLexer(make_ref<Lex::Lexer>(content, m_Preprocessor));
	m_Parser.ConsumeToken();
	m_CurrentScope = m_Sema.GetCurrentScope();
	const auto stmt = m_Parser.ParseStatement();
	if (!stmt || m_DiagConsumer->IsErrored())
	{
		m_DiagConsumer->Reset();
		nat_Throw(InterpreterException, u8"编译语句 \"{0}\" 失败"_nv, content);
	}

	m_Visitor->Visit(stmt);
	m_Visitor->ResetReturnedExpr();
	m_DeclStorage.GarbageCollect();
}

natRefPointer<Semantic::Scope> Interpreter::GetScope() const noexcept
{
	return m_CurrentScope;
}
