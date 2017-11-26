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
	// 解释器的 REPL 模式将视为第2阶段
	m_Sema.SetCurrentPhase(Semantic::Sema::Phase::Phase2);

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

Interpreter::InterpreterDeclStorage& Interpreter::GetDeclStorage() noexcept
{
	return m_DeclStorage;
}

void Interpreter::RegisterFunction(nStrView name, Type::TypePtr resultType, std::initializer_list<Type::TypePtr> argTypes, Function const& func)
{
	Lex::Token dummyToken;
	const auto id = m_Parser.GetPreprocessor().FindIdentifierInfo(name, dummyToken);
	
	auto scope = m_Sema.GetCurrentScope();
	for (; scope->GetParent(); scope = scope->GetParent()) {}

	const auto dc = scope->GetEntity();

	auto funcType = make_ref<Type::FunctionType>(from(argTypes), resultType);
	auto funcDecl = make_ref<Declaration::FunctionDecl>(Declaration::Decl::Function, dc,
		SourceLocation{}, SourceLocation{}, id, funcType, Specifier::StorageClass::Extern);

	Declaration::DeclContext* const funcDc = funcDecl.Get();

	funcDecl->SetParams(from(argTypes).select([funcDc](Type::TypePtr const& argType)
	{
		return make_ref<Declaration::ParmVarDecl>(Declaration::Decl::ParmVar, funcDc,
			SourceLocation{}, SourceLocation{}, nullptr, argType, Specifier::StorageClass::None, nullptr);
	}));

	m_Sema.PushOnScopeChains(funcDecl, scope);
	m_FunctionMap.emplace(std::move(funcDecl), func);
}

ASTContext& Interpreter::GetASTContext() noexcept
{
	return m_AstContext;
}
