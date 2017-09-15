#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;

Interpreter::InterpreterDiagIdMap::InterpreterDiagIdMap(natRefPointer<TextReader<StringType::Utf8>> const& reader)
{
	using DiagIDUnderlyingType = std::underlying_type_t<Diag::DiagnosticsEngine::DiagID>;

	std::unordered_map<nStrView, Diag::DiagnosticsEngine::DiagID> idNameMap;
	for (auto id = DiagIDUnderlyingType{}; id < static_cast<DiagIDUnderlyingType>(Diag::DiagnosticsEngine::DiagID::EndOfDiagID); ++id)
	{
		if (auto idName = Diag::DiagnosticsEngine::GetDiagIDName(static_cast<Diag::DiagnosticsEngine::DiagID>(id)))
		{
			idNameMap.emplace(idName, static_cast<Diag::DiagnosticsEngine::DiagID>(id));
		}
	}

	nString diagIDName;
	while (true)
	{
		auto curLine = reader->ReadLine();

		if (diagIDName.IsEmpty())
		{
			if (curLine.IsEmpty())
			{
				break;
			}

			diagIDName = std::move(curLine);
		}
		else
		{
			const auto iter = idNameMap.find(curLine);
			if (iter == idNameMap.cend())
			{
				// TODO: 报告错误的 ID
				break;
			}

			auto [i, succeed] = m_IdMap.try_emplace(iter->second, std::move(curLine));

			if (!succeed)
			{
				// TODO: 报告重复的 ID
			}
		}
	}
}

Interpreter::InterpreterDiagIdMap::~InterpreterDiagIdMap()
{
}

nString Interpreter::InterpreterDiagIdMap::GetText(Diag::DiagnosticsEngine::DiagID id)
{
	const auto iter = m_IdMap.find(id);
	return iter == m_IdMap.cend() ? "(No available text)" : iter->second;
}

Interpreter::InterpreterDiagConsumer::InterpreterDiagConsumer(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
}

Interpreter::InterpreterDiagConsumer::~InterpreterDiagConsumer()
{
}

void Interpreter::InterpreterDiagConsumer::HandleDiagnostic(Diag::DiagnosticsEngine::Level level,
	Diag::DiagnosticsEngine::Diagnostic const& diag)
{
	const auto levelId = static_cast<nuInt>(level);
	m_Interpreter.m_Logger.Log(levelId, diag.GetDiagMessage());

	const auto loc = diag.GetSourceLocation();
	if (loc.GetFileID())
	{
		auto [succeed, fileContent] = m_Interpreter.m_SourceManager.GetFileContent(loc.GetFileID());
		if (const auto line = loc.GetLineInfo(); succeed && line)
		{
			std::size_t offset{};
			for (nuInt i = 1; i < line; ++i)
			{
				offset = fileContent.Find(Environment::GetNewLine(), static_cast<std::ptrdiff_t>(offset));
				if (offset == nStrView::npos)
				{
					// TODO: 无法定位到源文件
					return;
				}

				offset += Environment::GetNewLine().GetSize();
			}

			const auto nextNewLine = fileContent.Find(Environment::GetNewLine(), static_cast<std::ptrdiff_t>(offset));
			const auto column = loc.GetColumnInfo();
			offset += column ? column - 1 : 0;
			if (nextNewLine <= offset)
			{
				// TODO: 无法定位到源文件
				return;
			}

			m_Interpreter.m_Logger.Log(levelId, fileContent.Slice(static_cast<std::ptrdiff_t>(offset), nextNewLine == nStrView::npos ? -1 : static_cast<std::ptrdiff_t>(nextNewLine)));
			m_Interpreter.m_Logger.Log(levelId, "^");
		}
	}
}

Interpreter::InterpreterASTConsumer::InterpreterASTConsumer(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
}

Interpreter::InterpreterASTConsumer::~InterpreterASTConsumer()
{
}

void Interpreter::InterpreterASTConsumer::Initialize(ASTContext& /*context*/)
{
}

void Interpreter::InterpreterASTConsumer::HandleTranslationUnit(ASTContext& context)
{
	const auto iter = m_NamedDecls.find("Main");
	natRefPointer<Declaration::FunctionDecl> mainDecl;
	if (iter == m_NamedDecls.cend() || !((mainDecl = iter->second)))
	{
		nat_Throw(InterpreterException, u8"无法找到名为 Main 的函数");
	}

	if (mainDecl->Decl::GetType() != Declaration::Decl::Function)
	{
		nat_Throw(InterpreterException, u8"找到了名为 Main 的方法，但需要一个函数");
	}

	m_Interpreter.m_Visitor->Visit(mainDecl->GetBody());
}

nBool Interpreter::InterpreterASTConsumer::HandleTopLevelDecl(Linq<Valued<Declaration::DeclPtr>> const& decls)
{
	for (auto&& decl : decls)
	{
		if (auto namedDecl = static_cast<natRefPointer<Declaration::NamedDecl>>(decl))
		{
			m_NamedDecls.emplace(namedDecl->GetIdentifierInfo()->GetName(), std::move(namedDecl));
		}

		m_UnnamedDecls.emplace_back(std::move(decl));
	}

	return true;
}

Interpreter::InterpreterStmtVisitor::InterpreterStmtVisitor(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
}

Interpreter::InterpreterStmtVisitor::~InterpreterStmtVisitor()
{
}

void Interpreter::InterpreterStmtVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此语句无法被访问");
}

void Interpreter::InterpreterStmtVisitor::VisitBreakStmt(natRefPointer<Statement::BreakStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitCatchStmt(natRefPointer<Statement::CatchStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitTryStmt(natRefPointer<Statement::TryStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitCompoundStmt(natRefPointer<Statement::CompoundStmt> const& stmt)
{
	for (auto&& item : stmt->GetChildrens())
	{
		Visit(item);
	}
}

void Interpreter::InterpreterStmtVisitor::VisitContinueStmt(natRefPointer<Statement::ContinueStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitDeclStmt(natRefPointer<Statement::DeclStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitForStmt(NatsuLib::natRefPointer<Statement::ForStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitGotoStmt(NatsuLib::natRefPointer<Statement::GotoStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitIfStmt(NatsuLib::natRefPointer<Statement::IfStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt)
{
	Visit(stmt->GetSubStmt());
}

void Interpreter::InterpreterStmtVisitor::VisitNullStmt(NatsuLib::natRefPointer<Statement::NullStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitCaseStmt(NatsuLib::natRefPointer<Statement::CaseStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitDefaultStmt(NatsuLib::natRefPointer<Statement::DefaultStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitSwitchStmt(NatsuLib::natRefPointer<Statement::SwitchStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitWhileStmt(NatsuLib::natRefPointer<Statement::WhileStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

void Interpreter::InterpreterStmtVisitor::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现");
}

Interpreter::Interpreter(natRefPointer<TextReader<StringType::Utf8>> const& diagIdMapFile, natLog& logger)
	: m_Diag{ make_ref<InterpreterDiagIdMap>(diagIdMapFile), make_ref<InterpreterDiagConsumer>(*this) },
	m_Logger{ logger },
	m_SourceManager{ m_Diag, m_FileManager },
	m_Preprocessor{ m_Diag, m_SourceManager },
	m_Sema{ m_Preprocessor, m_AstContext, m_Consumer = make_ref<InterpreterASTConsumer>(*this) },
	m_Parser{ m_Preprocessor, m_Sema },
	m_Visitor{ make_ref<InterpreterStmtVisitor>(*this) }
{
}

Interpreter::~Interpreter()
{
}

void Interpreter::Run(Uri uri)
{
	nat_Throw(NotImplementedException);
}

void Interpreter::Run(nStrView content)
{
	nat_Throw(NotImplementedException);
}
