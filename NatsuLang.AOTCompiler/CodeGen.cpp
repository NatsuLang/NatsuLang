#include "CodeGen.h"
#include "Serialization.h"

#include <natLocalFileScheme.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4141 4146 4244 4267 4291 4624 4996)
#endif // _MSC_VER

#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

#include <Sema/DefaultActions.h>

using namespace NatsuLib;
using namespace NatsuLang;
using namespace Compiler;

namespace
{
	class NameManglingRuleAttribute
		: public natRefObjImpl<NameManglingRuleAttribute, Declaration::IAttribute>
	{
	public:
		virtual nString GetMangledName(natRefPointer<Declaration::NamedDecl> const& decl) = 0;
	};

	class BuiltinAttribute
		: public natRefObjImpl<BuiltinAttribute, Declaration::IAttribute>
	{
	public:
		nStrView GetName() const noexcept override
		{
			return u8"Builtin"_nv;
		}
	};

	class CallingConventionAttribute
		: public natRefObjImpl<CallingConventionAttribute, Declaration::IAttribute>
	{
	public:
		class CallingConventionAttributeSerializer
			: public natRefObjImpl<CallingConventionAttributeSerializer, IAttributeSerializer>
		{
		public:
			void Serialize(natRefPointer<IAttribute> const& attribute, natRefPointer<ISerializationArchiveWriter> const& writer) override
			{
				const auto callingConvention = attribute.Cast<CallingConventionAttribute>();
				assert(callingConvention);
				writer->WriteNumType(u8"CallingConvention"_nv, callingConvention->GetCallingConvention());
			}

			natRefPointer<IAttribute> Deserialize(natRefPointer<ISerializationArchiveReader> const& reader) override
			{
				CallingConvention callingConvention;
				if (!reader->ReadNumType(u8"CallingConvention", callingConvention))
				{
					nat_Throw(AotCompilerException, u8"无法读取调用约定的值"_nv);
				}

				return make_ref<CallingConventionAttribute>(callingConvention);
			}
		};

		enum class CallingConvention
		{
			Cdecl,
			Stdcall
		};

		explicit CallingConventionAttribute(CallingConvention callingConvention)
			: m_CallingConvention{ callingConvention }
		{
		}

		nStrView GetName() const noexcept override
		{
			return u8"CallingConvention"_nv;
		}

		CallingConvention GetCallingConvention() const noexcept
		{
			return m_CallingConvention;
		}

		static constexpr llvm::CallingConv::ID ToLLVMCallingConv(CallingConvention value) noexcept
		{
			switch (value)
			{
			case CallingConvention::Cdecl:
				return llvm::CallingConv::C;
			case CallingConvention::Stdcall:
				return llvm::CallingConv::X86_StdCall;
			default:
				assert(!"Invalid CallingConvention.");
				std::terminate();
			}
		}

	private:
		CallingConvention m_CallingConvention;
	};

	class ActionCallingConvention
		: public natRefObjImpl<ActionCallingConvention, ICompilerAction>
	{
	public:
		nStrView GetName() const noexcept override
		{
			return u8"CallingConvention"_nv;
		}

		natRefPointer<IActionContext> StartAction(CompilerActionContext const& context) override
		{
			auto actionContext = make_ref<ActionCallingConventionContext>();
			actionContext->Diag = &context.GetParser().GetDiagnosticsEngine();
			return actionContext;
		}

		void EndAction(natRefPointer<IActionContext> const& context, std::function<nBool(natRefPointer<ASTNode>)> const& output) override
		{
			const auto actionContext = context.UnsafeCast<ActionCallingConventionContext>();

			assert(actionContext->Decl);
			actionContext->Decl->AttachAttribute(make_ref<CallingConventionAttribute>(actionContext->CallingConvention));

			if (output)
			{
				output(actionContext->Decl);
			}
		}

	private:
		struct ActionCallingConventionContext
			: natRefObjImpl<ActionCallingConventionContext, IActionContext>
		{
			ActionCallingConventionContext()
				: Diag{}, AssignedCallingConvention{},
				  CallingConvention{ CallingConventionAttribute::CallingConvention::Cdecl }
			{
			}

			natRefPointer<IArgumentRequirement> GetArgumentRequirement() override
			{
				return make_ref<SimpleArgumentRequirement>(std::initializer_list<CompilerActionArgumentType>{ CompilerActionArgumentType::Identifier, CompilerActionArgumentType::MayBeUnresolved | CompilerActionArgumentType::Declaration | CompilerActionArgumentType::MayBeSingle });
			}

			void AddArgument(natRefPointer<ASTNode> const& arg) override
			{
				if (!AssignedCallingConvention)
				{
					const auto idDecl = arg.Cast<Declaration::UnresolvedDecl>();
					if (!idDecl)
					{
						Diag->Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier);
						return;
					}

					const auto id = idDecl->GetName();
					if (id == "Stdcall")
					{
						CallingConvention = CallingConventionAttribute::CallingConvention::Stdcall;
					}
					else
					{
						assert(id == "Cdecl");
						CallingConvention = CallingConventionAttribute::CallingConvention::Cdecl;
					}
					AssignedCallingConvention = true;
				}
				else
				{
					Decl = arg;
					if (!Decl)
					{
						Diag->Report(Diag::DiagnosticsEngine::DiagID::ErrExpected).AddArgument("Declaration");
					}
				}
			}

			Diag::DiagnosticsEngine* Diag;
			nBool AssignedCallingConvention;
			CallingConventionAttribute::CallingConvention CallingConvention;
			Declaration::DeclPtr Decl;
		};
	};

	nString GetQualifiedName(natRefPointer<Declaration::NamedDecl> const& decl, nBool isNameMangling = true)
	{
		if (isNameMangling)
		{
			if (const auto query = decl->GetAttributes<NameManglingRuleAttribute>(); !query.empty())
			{
				const auto attr = query.first();
				return attr->GetMangledName(decl);
			}
		}

		nString qualifiedName = decl->GetName();
		auto dc = decl->GetContext();

		nString prefix;

		while (const auto scopeDecl = Declaration::Decl::CastFromDeclContext(dc))
		{
			if (scopeDecl->GetType() == Declaration::Decl::TranslationUnit)
			{
				break;
			}

			prefix.Clear();  // NOLINT

			if (const auto namedDecl = dynamic_cast<Declaration::NamedDecl*>(scopeDecl))
			{
				prefix = namedDecl->GetName();
			}
			else
			{
				prefix.Append(u8'(');
				prefix.Append(Declaration::Decl::GetDeclTypeName(scopeDecl->GetType()));
				prefix.Append(u8')');
			}

			prefix.Append(u8'.');
			prefix.Append(qualifiedName);
			qualifiedName = std::move(prefix);

			dc = scopeDecl->GetContext();
		}

		return qualifiedName;
	}
}

AotCompiler::AotDiagIdMap::AotDiagIdMap(natRefPointer<TextReader<StringType::Utf8>> const& reader)
{
	using DiagIDUnderlyingType = std::underlying_type_t<Diag::DiagnosticsEngine::DiagID>;

	std::unordered_map<nStrView, Diag::DiagnosticsEngine::DiagID> idNameMap;
	for (auto id = static_cast<DiagIDUnderlyingType>(Diag::DiagnosticsEngine::DiagID::Invalid) + 1;
		id < static_cast<DiagIDUnderlyingType>(Diag::DiagnosticsEngine::DiagID::EndOfDiagID); ++id)
	{
		if (auto idName = Diag::DiagnosticsEngine::GetDiagIDName(static_cast<Diag::DiagnosticsEngine::DiagID>(id)))
		{
			idNameMap.emplace(idName, static_cast<Diag::DiagnosticsEngine::DiagID>(id));
		}
	}

	reader->SetNewLine(u8"\n"_nv);

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
			const auto iter = idNameMap.find(diagIDName);
			if (iter == idNameMap.cend())
			{
				// TODO: 报告错误的 ID
				break;
			}

			if (!m_IdMap.try_emplace(iter->second, std::move(curLine)).second)
			{
				// TODO: 报告重复的 ID
			}

			diagIDName.Clear();
		}
	}
}

AotCompiler::AotDiagIdMap::~AotDiagIdMap()
{
}

nString AotCompiler::AotDiagIdMap::GetText(Diag::DiagnosticsEngine::DiagID id)
{
	const auto iter = m_IdMap.find(id);
	return iter == m_IdMap.cend() ? u8"(No available text)"_nv : iter->second.GetView();
}

AotCompiler::AotDiagConsumer::AotDiagConsumer(AotCompiler& compiler)
	: m_Compiler{ compiler }, m_Errored{ false }
{
}

AotCompiler::AotDiagConsumer::~AotDiagConsumer()
{
}

void AotCompiler::AotDiagConsumer::HandleDiagnostic(Diag::DiagnosticsEngine::Level level, Diag::DiagnosticsEngine::Diagnostic const& diag)
{
	m_Errored |= Diag::DiagnosticsEngine::IsUnrecoverableLevel(level);

	nuInt levelId;

	switch (level)
	{
	case Diag::DiagnosticsEngine::Level::Ignored:
	case Diag::DiagnosticsEngine::Level::Note:
	case Diag::DiagnosticsEngine::Level::Remark:
		levelId = natLog::Msg;
		break;
	case Diag::DiagnosticsEngine::Level::Warning:
		levelId = natLog::Warn;
		break;
	case Diag::DiagnosticsEngine::Level::Error:
	case Diag::DiagnosticsEngine::Level::Fatal:
	default:
		levelId = natLog::Err;
		break;
	}

	m_Compiler.m_Logger.Log(levelId, diag.GetDiagMessage());

	const auto range = diag.GetSourceRange();
	// 显示 range 的以后再做。。
	const auto loc = range.GetBegin();
	if (loc.GetFileID())
	{
		const auto fileUri = m_Compiler.m_SourceManager.FindFileUri(loc.GetFileID());
		const auto [line, range] = m_Compiler.m_Preprocessor.GetLexer()->GetLine(loc);
		if (range.IsValid())
		{
			m_Compiler.m_Logger.Log(levelId, u8"在文件 \"{0}\"，第 {1} 行："_nv, fileUri.empty() ? u8"未知"_nv : fileUri, line + 1);
			m_Compiler.m_Logger.Log(levelId, nStrView{ range.GetBegin().GetPos(), range.GetEnd().GetPos() });
			nString indentation(u8' ', loc.GetPos() - range.GetBegin().GetPos());
			m_Compiler.m_Logger.Log(levelId, u8"{0}^"_nv, indentation);
		}
	}
}

AotCompiler::AotAstConsumer::AotAstConsumer(AotCompiler& compiler)
	: m_Compiler{ compiler }
{
}

AotCompiler::AotAstConsumer::~AotAstConsumer()
{
}

void AotCompiler::AotAstConsumer::Initialize(ASTContext& context)
{
}

void AotCompiler::AotAstConsumer::HandleTranslationUnit(ASTContext& context)
{
}

nBool AotCompiler::AotAstConsumer::HandleTopLevelDecl(Linq<Valued<Declaration::DeclPtr>> const& decls)
{
	if (m_Compiler.m_DiagConsumer->IsErrored())
	{
		return false;
	}

	auto declVec = decls.select([](Declaration::DeclPtr decl)
	{
		return std::pair<Declaration::DeclPtr, llvm::Value*>(std::move(decl), nullptr);
	}).Cast<std::vector<std::pair<Declaration::DeclPtr, llvm::Value*>>>();

	// 生成声明
	for (auto& decl : declVec)
	{
		if (auto funcDecl = decl.first.Cast<Declaration::FunctionDecl>())
		{
			const auto functionType = llvm::dyn_cast<llvm::FunctionType>(m_Compiler.getCorrespondingType(funcDecl));
			assert(functionType);

			const auto functionName = GetQualifiedName(funcDecl);

			const auto funcValue = llvm::Function::Create(functionType,
				llvm::GlobalVariable::ExternalLinkage,
				llvm::StringRef{ functionName.cbegin(), functionName.size() },
				m_Compiler.m_Module.get());

			const auto query = funcDecl->GetAttributes<CallingConventionAttribute>();

			if (!query.empty())
			{
				const auto attr = query.first();
				funcValue->setCallingConv(CallingConventionAttribute::ToLLVMCallingConv(attr->GetCallingConvention()));
			}

			auto argIter = funcValue->arg_begin();
			const auto argEnd = funcValue->arg_end();
			auto paramIter = funcDecl->GetParams().begin();
			const auto paramEnd = funcDecl->GetParams().end();

			if (funcDecl.Cast<Declaration::MethodDecl>())
			{
				// 成员函数的第一个参数是 this，其类型应当是指向该类的指针
				argIter->setName("this");
				++argIter;
			}

			for (; argIter != argEnd && paramIter != paramEnd; ++argIter, static_cast<void>(++paramIter))
			{
				const auto name = (*paramIter)->GetIdentifierInfo()->GetName();
				argIter->setName(llvm::StringRef{ name.cbegin(), name.size() });
			}

			m_Compiler.m_FunctionMap.emplace(std::move(funcDecl), funcValue);
			decl.second = funcValue;
		}
		else if (auto varDecl = decl.first.Cast<Declaration::VarDecl>(); varDecl && varDecl->GetStorageClass() != Specifier::StorageClass::Const)
		{
			const auto varType = m_Compiler.getCorrespondingType(varDecl->GetValueType());

			const auto varName = GetQualifiedName(varDecl);

			// TODO: 修改成正儿八经的初始化

			llvm::Constant* initValue{};

			const auto initializer = varDecl->GetInitializer();
			auto isConstant = true;

			if (initializer)
			{
				Expression::Expr::EvalResult evalResult;
				if (initializer->Evaluate(evalResult, m_Compiler.m_AstContext))
				{
					if (evalResult.Result.index() == 0)
					{
						initValue = llvm::ConstantInt::get(varType, std::get<0>(evalResult.Result));
					}
					else
					{
						initValue = llvm::ConstantFP::get(varType, std::get<1>(evalResult.Result));
					}
				}
			}
			else
			{
				if (varDecl->GetStorageClass() == Specifier::StorageClass::Extern)
				{
					isConstant = false;
				}
			}

			const auto varValue = new llvm::GlobalVariable(*m_Compiler.m_Module, varType, isConstant, llvm::GlobalValue::ExternalLinkage, initValue, llvm::StringRef{ varName.data(), varName.size() });
			m_Compiler.m_GlobalVariableMap.emplace(std::move(varDecl), varValue);
			decl.second = varValue;
		}
		else if (const auto dc = Declaration::Decl::CastToDeclContext(decl.first.Get()))
		{
			HandleTopLevelDecl(dc->GetDecls());
		}
	}

	// 生成定义
	for (auto const& decl : declVec)
	{
		if (const auto funcDecl = decl.first.Cast<Declaration::FunctionDecl>())
		{
			switch (funcDecl->GetStorageClass())
			{
			case Specifier::StorageClass::None:
			{
				AotStmtVisitor visitor{ m_Compiler, funcDecl, llvm::dyn_cast<llvm::Function>(decl.second) };
				visitor.StartVisit();
				visitor.EndVisit();
				break;
			}
			case Specifier::StorageClass::Extern:
				break;
			default:
				break;
			}
		}
		else if (const auto varDecl = decl.first.Cast<Declaration::VarDecl>(); varDecl && varDecl->GetStorageClass() != Specifier::StorageClass::Const)
		{
			// TODO: 动态初始化
		}
	}

	return true;
}

AotCompiler::AotStmtVisitor::AotStmtVisitor(AotCompiler& compiler, natRefPointer<Declaration::FunctionDecl> funcDecl, llvm::Function* funcValue)
	: m_Compiler{ compiler }, m_CurrentFunction{ std::move(funcDecl) }, m_CurrentFunctionValue{ funcValue },
	  m_This{}, m_LastVisitedValue{}, m_RequiredModifiableValue{}, m_CurrentLexicalScope{},
	  m_ReturnBlock{ llvm::BasicBlock::Create(compiler.m_LLVMContext, "Return"), m_CleanupStack.begin(), true },
	  m_ReturnValue{}
{
	auto argIter = m_CurrentFunctionValue->arg_begin();
	const auto argEnd = m_CurrentFunctionValue->arg_end();
	auto paramIter = m_CurrentFunction->GetParams().begin();
	const auto paramEnd = m_CurrentFunction->GetParams().end();

	if (m_CurrentFunction.Cast<Declaration::MethodDecl>())
	{
		m_This = &*argIter++;
	}

	const auto block = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "Entry", m_CurrentFunctionValue);
	m_Compiler.m_IRBuilder.SetInsertPoint(block);

	for (; argIter != argEnd && paramIter != paramEnd; ++argIter, static_cast<void>(++paramIter))
	{
		llvm::IRBuilder<> entryIRBuilder{
			&m_CurrentFunctionValue->getEntryBlock(), m_CurrentFunctionValue->getEntryBlock().begin()
		};
		const auto arg = entryIRBuilder.CreateAlloca(argIter->getType(), nullptr, argIter->getName());
		m_Compiler.m_IRBuilder.CreateStore(&*argIter, arg);

		m_DeclMap.emplace(*paramIter, arg);
	}

	const auto retType = m_CurrentFunction->GetValueType().UnsafeCast<Type::FunctionType>()->GetResultType();
	assert(retType);
	if (!retType->IsVoid())
	{
		m_ReturnValue = m_Compiler.m_IRBuilder.CreateAlloca(m_Compiler.getCorrespondingType(retType), nullptr, "ret");
	}
}

AotCompiler::AotStmtVisitor::~AotStmtVisitor()
{
}

void AotCompiler::AotStmtVisitor::VisitInitListExpr(natRefPointer<Expression::InitListExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"初始化列表不应当作为表达式求值"_nv);
}

void AotCompiler::AotStmtVisitor::VisitBreakStmt(natRefPointer<Statement::BreakStmt> const& stmt)
{
	assert(!m_BreakContinueStack.empty() && "break not in a breakable scope");

	const auto dest = m_BreakContinueStack.back().first;
	assert(dest.GetBlock());
	EmitBranchWithCleanup(dest);
}

void AotCompiler::AotStmtVisitor::VisitCatchStmt(natRefPointer<Statement::CatchStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitTryStmt(natRefPointer<Statement::TryStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCompoundStmt(natRefPointer<Statement::CompoundStmt> const& stmt)
{
	EmitCompoundStmt(stmt);
}

void AotCompiler::AotStmtVisitor::VisitContinueStmt(natRefPointer<Statement::ContinueStmt> const& stmt)
{
	assert(!m_BreakContinueStack.empty() && "continue not in a continuable scope");

	const auto dest = m_BreakContinueStack.back().second;
	assert(dest.GetBlock());
	EmitBranchWithCleanup(dest);
}

void AotCompiler::AotStmtVisitor::VisitDeclStmt(natRefPointer<Statement::DeclStmt> const& stmt)
{
	const auto decl = stmt->GetDecl();
	if (!decl)
	{
		nat_Throw(AotCompilerException, u8"错误的声明"_nv);
	}

	if (const auto varDecl = decl.Cast<Declaration::VarDecl>())
	{
		if (varDecl->IsFunction())
		{
			// 目前只能在顶层声明函数
			return;
		}

		EmitVarDecl(varDecl);
	}
}

void AotCompiler::AotStmtVisitor::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	const auto loopEnd = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "do.end");
	const auto loopCond = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "do.cond");

	m_BreakContinueStack.emplace_back(JumpDest{ loopEnd, GetCleanupStackTop(), true }, JumpDest{ loopCond, GetCleanupStackTop(), true });

	const auto loopBody = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "do.body");
	EmitBlock(loopBody);
	{
		const auto loopBodyStmt = stmt->GetBody();
		LexicalScope scope{ *this, { loopBodyStmt->GetStartLoc(), loopBodyStmt->GetEndLoc() } };
		if (loopBodyStmt->GetType() == Statement::Stmt::CompoundStmtClass)
		{
			scope.SetAlreadyCleaned();
		}
		Visit(loopBodyStmt);
	}

	m_BreakContinueStack.pop_back();

	EmitBlock(loopCond);

	EvaluateAsBool(stmt->GetCond());
	const auto cond = m_LastVisitedValue;

	// do {} while (false) 较为常用，针对这个场景优化
	auto alwaysFalse = false;
	if (const auto val = llvm::dyn_cast<llvm::Constant>(cond))
	{
		if (val->isNullValue())
		{
			alwaysFalse = true;
		}
	}

	if (!alwaysFalse)
	{
		m_Compiler.m_IRBuilder.CreateCondBr(cond, loopBody, loopEnd);
	}

	EmitBlock(loopEnd);
}

void AotCompiler::AotStmtVisitor::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitConditionalOperator(natRefPointer<Expression::ConditionalOperator> const& expr)
{
	auto lhsBlock = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "cond.true");
	auto rhsBlock = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "cond.false");
	const auto endBlock = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "cond.end");

	EvaluateAsBool(expr->GetCondition());
	const auto cond = m_LastVisitedValue;

	m_Compiler.m_IRBuilder.CreateCondBr(cond, lhsBlock, rhsBlock);

	EmitBlock(lhsBlock);

	EvaluateValue(expr->GetLeftOperand());
	const auto lhs = m_LastVisitedValue;

	lhsBlock = m_Compiler.m_IRBuilder.GetInsertBlock();

	if (lhs)
	{
		m_Compiler.m_IRBuilder.CreateBr(endBlock);
	}

	EmitBlock(rhsBlock);

	EvaluateValue(expr->GetRightOperand());
	const auto rhs = m_LastVisitedValue;

	rhsBlock = m_Compiler.m_IRBuilder.GetInsertBlock();

	EmitBlock(endBlock);

	if (lhs && rhs)
	{
		const auto phi = m_Compiler.m_IRBuilder.CreatePHI(m_Compiler.getCorrespondingType(expr->GetExprType()), 2, "condvalue");
		phi->addIncoming(lhs, lhsBlock);
		phi->addIncoming(rhs, rhsBlock);

		setLastVisitedResult(phi);
	}
	else
	{
		nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
	}
}

void AotCompiler::AotStmtVisitor::VisitArraySubscriptExpr(natRefPointer<Expression::ArraySubscriptExpr> const& expr)
{
	EvaluateAsModifiableValue(expr->GetLeftOperand());
	const auto baseExpr = m_LastVisitedValue;
	EvaluateValue(expr->GetRightOperand());
	const auto indexExpr = m_LastVisitedValue;

	m_LastVisitedValue = m_Compiler.m_IRBuilder.CreateGEP(baseExpr,
		{ llvm::ConstantInt::getNullValue(llvm::Type::getInt64Ty(m_Compiler.m_LLVMContext)), indexExpr }, "arrayElemPtr");

	if (!m_RequiredModifiableValue)
	{
		setLastVisitedResult(m_Compiler.m_IRBuilder.CreateLoad(m_LastVisitedValue, "arrayElem"));
	}
}

void AotCompiler::AotStmtVisitor::VisitBinaryOperator(natRefPointer<Expression::BinaryOperator> const& expr)
{
	EvaluateValue(expr->GetLeftOperand());
	const auto leftOperand = m_LastVisitedValue;

	EvaluateValue(expr->GetRightOperand());
	const auto rightOperand = m_LastVisitedValue;

	const auto opCode = expr->GetOpcode();
	// 左右操作数类型应当相同
	const auto commonType = expr->GetLeftOperand()->GetExprType();
	const auto resultType = expr->GetExprType().Cast<Type::BuiltinType>();

	setLastVisitedResult(EmitBinOp(leftOperand, rightOperand, opCode, expr->GetLeftOperand()->GetExprType(), expr->GetRightOperand()->GetExprType(), resultType));
}

void AotCompiler::AotStmtVisitor::VisitCompoundAssignOperator(natRefPointer<Expression::CompoundAssignOperator> const& expr)
{
	const auto resultType = expr->GetLeftOperand()->GetExprType();
	const auto rightType = expr->GetRightOperand()->GetExprType();

	EvaluateAsModifiableValue(expr->GetLeftOperand());
	const auto leftOperand = m_LastVisitedValue;

	EvaluateValue(expr->GetRightOperand());
	const auto rightOperand = m_LastVisitedValue;

	const auto opCode = expr->GetOpcode();
	llvm::Value* value;

	switch (opCode)
	{
	case Expression::BinaryOperationType::Assign:
		value = rightOperand;
		break;
#define COMPOUND_ASSIGN_OPERATION(Name, Spelling) \
	case Expression::BinaryOperationType::Name##Assign:\
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Name, resultType, rightType, resultType);\
		break;
#include "AST/OperationTypesDef.h"
	default:
		assert(!"Invalid Opcode");
		nat_Throw(AotCompilerException, u8"无效的 Opcode"_nv);
	}

	m_Compiler.m_IRBuilder.CreateStore(value, leftOperand);

	setLastVisitedResult(m_RequiredModifiableValue ? leftOperand : m_Compiler.m_IRBuilder.CreateLoad(leftOperand));
}

void AotCompiler::AotStmtVisitor::VisitBooleanLiteral(natRefPointer<Expression::BooleanLiteral> const& expr)
{
	setLastVisitedResult(llvm::ConstantInt::get(llvm::Type::getInt1Ty(m_Compiler.m_LLVMContext), expr->GetValue()));
}

void AotCompiler::AotStmtVisitor::VisitConstructExpr(natRefPointer<Expression::ConstructExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitDeleteExpr(natRefPointer<Expression::DeleteExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitNewExpr(natRefPointer<Expression::NewExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitThisExpr(natRefPointer<Expression::ThisExpr> const& /*expr*/)
{
	setLastVisitedResult(m_RequiredModifiableValue ? m_This : m_Compiler.m_IRBuilder.CreateLoad(m_This));
}

void AotCompiler::AotStmtVisitor::VisitThrowExpr(natRefPointer<Expression::ThrowExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCallExpr(natRefPointer<Expression::CallExpr> const& expr)
{
	EvaluateValue(expr->GetCallee());
	const auto callee = m_LastVisitedValue;
	natRefPointer<Declaration::FunctionDecl> funcDecl;
	if (auto func = m_LastVisitedDecl.Cast<Declaration::FunctionDecl>())
	{
		funcDecl = std::move(func);
	}
	llvm::FunctionType* calleeType;
	if (const auto ptrType = llvm::dyn_cast_or_null<llvm::PointerType>(m_LastVisitedValue->getType()); ptrType &&
		llvm::isa<llvm::FunctionType>(ptrType->getElementType()))
	{
		calleeType = llvm::cast<llvm::FunctionType>(ptrType->getElementType());
	}
	else
	{
		nat_Throw(AotCompilerException, u8"被调用者不是函数或者函数指针"_nv);
	}

	assert(callee);

	std::vector<llvm::Value*> args;
	args.reserve(expr->GetArgCount());

	for (auto&& arg : expr->GetArgs())
	{
		// TODO: 直接按位复制了，在需要的时候应由前端生成复制构造函数，但此处没有看到分配存储？
		// TODO: 搞清楚 C/C++ 的 abi 差异了，需要添加 abi 相关选项来控制
		EvaluateValue(arg);
		assert(m_LastVisitedValue);
		args.emplace_back(m_LastVisitedValue);
	}

	// 函数指针不支持默认参数
	// TODO: 禁止不在末尾的默认参数及在可变参数之前的默认参数
	if (funcDecl && funcDecl->GetParamCount() > args.size())
	{
		for (auto&& param : funcDecl->GetParams().skip(args.size()))
		{
			const auto defaultArg = param->GetInitializer();
			assert(defaultArg);
			EvaluateValue(defaultArg);
			assert(m_LastVisitedValue);
			args.emplace_back(m_LastVisitedValue);
		}
	}

	const auto callInst = m_Compiler.m_IRBuilder.CreateCall(callee, args);

	if (m_LastVisitedDecl)
	{
		// TODO: 考虑在类型上加属性，这样可以把调用约定写在类型里
		const auto query = m_LastVisitedDecl->GetAttributes<CallingConventionAttribute>();
		if (!query.empty())
		{
			callInst->setCallingConv(CallingConventionAttribute::ToLLVMCallingConv(query.first()->GetCallingConvention()));
		}
	}

	if (!calleeType->getReturnType()->isVoidTy())
	{
		callInst->setName("ret");
	}

	setLastVisitedResult(callInst);
}

void AotCompiler::AotStmtVisitor::VisitMemberCallExpr(natRefPointer<Expression::MemberCallExpr> const& expr)
{
	EvaluateValue(expr->GetCallee());
	// 暂时不支持成员函数指针
	const auto callee = llvm::cast<llvm::Function>(m_LastVisitedValue);
	natRefPointer<Declaration::MethodDecl> funcDecl;
	if (auto func = m_LastVisitedDecl.Cast<Declaration::MethodDecl>())
	{
		funcDecl = std::move(func);
	}
	assert(callee);

	const auto baseObj = expr->GetImplicitObjectArgument();
	if (Type::Type::GetUnderlyingType(baseObj->GetExprType())->GetType() == Type::Type::Pointer)
	{
		// 是指针，直接取值
		EvaluateValue(baseObj);
	}
	else
	{
		// 是对象，取地址
		EvaluateAsModifiableValue(baseObj);
	}

	const auto baseObjValue = m_LastVisitedValue;

	assert(callee && baseObjValue);

	// TODO: 消除重复代码

	// 将基础对象的引用作为第一个参数传入
	std::vector<llvm::Value*> args{ baseObjValue };
	args.reserve(expr->GetArgCount() + 1);

	for (auto&& arg : expr->GetArgs())
	{
		// TODO: 直接按位复制了，在需要的时候应由前端生成复制构造函数，但此处没有看到分配存储？
		EvaluateValue(arg);
		assert(m_LastVisitedValue);
		args.emplace_back(m_LastVisitedValue);
	}

	// 函数指针不支持默认参数
	// TODO: 禁止不在末尾的默认参数及在可变参数之前的默认参数
	if (funcDecl && funcDecl->GetParamCount() > args.size() - 1)
	{
		for (auto&& param : funcDecl->GetParams().skip(args.size() - 1))
		{
			const auto defaultArg = param->GetInitializer();
			assert(defaultArg);
			EvaluateValue(defaultArg);
			assert(m_LastVisitedValue);
			args.emplace_back(m_LastVisitedValue);
		}
	}

	const auto callInst = m_Compiler.m_IRBuilder.CreateCall(callee, args);

	if (m_LastVisitedDecl)
	{
		// TODO: 考虑在类型上加属性，这样可以把调用约定写在类型里
		const auto query = m_LastVisitedDecl->GetAttributes<CallingConventionAttribute>();
		if (!query.empty())
		{
			callInst->setCallingConv(CallingConventionAttribute::ToLLVMCallingConv(query.first()->GetCallingConvention()));
		}
	}

	if (!callee->getReturnType()->isVoidTy())
	{
		callInst->setName("ret");
	}

	setLastVisitedResult(callInst);
}

void AotCompiler::AotStmtVisitor::VisitCastExpr(natRefPointer<Expression::CastExpr> const& expr)
{
	const auto operand = expr->GetOperand();
	EvaluateValue(operand);
	setLastVisitedResult(ConvertScalarTo(m_LastVisitedValue, operand->GetExprType(), expr->GetExprType()));
}

void AotCompiler::AotStmtVisitor::VisitAsTypeExpr(natRefPointer<Expression::AsTypeExpr> const& expr)
{
	const auto operand = expr->GetOperand();
	EvaluateValue(operand);
	setLastVisitedResult(ConvertScalarTo(m_LastVisitedValue, operand->GetExprType(), expr->GetExprType()));
}

void AotCompiler::AotStmtVisitor::VisitImplicitCastExpr(natRefPointer<Expression::ImplicitCastExpr> const& expr)
{
	const auto operand = expr->GetOperand();
	EvaluateValue(operand);
	setLastVisitedResult(ConvertScalarTo(m_LastVisitedValue, operand->GetExprType(), expr->GetExprType()));
}

void AotCompiler::AotStmtVisitor::VisitCharacterLiteral(natRefPointer<Expression::CharacterLiteral> const& expr)
{
	setLastVisitedResult(llvm::ConstantInt::get(m_Compiler.getCorrespondingType(expr->GetExprType()), expr->GetCodePoint()));
}

void AotCompiler::AotStmtVisitor::VisitDeclRefExpr(natRefPointer<Expression::DeclRefExpr> const& expr)
{
	const auto decl = expr->GetDecl();

	if (const auto varDecl = decl.Cast<Declaration::VarDecl>())
	{
		if (const auto funcDecl = varDecl.Cast<Declaration::FunctionDecl>())
		{
			EmitFunctionAddr(funcDecl);
			setLastVisitedResult(m_LastVisitedValue, decl);
		}
		else
		{
			if (varDecl->GetStorageClass() == Specifier::StorageClass::Const)
			{
				if (m_RequiredModifiableValue)
				{
					nat_Throw(AotCompilerException, u8"此定义不可变"_nv);
				}

				EvaluateValue(varDecl->GetInitializer());
				return;
			}

			EmitAddressOfVar(varDecl);
			if (!m_RequiredModifiableValue)
			{
				setLastVisitedResult(m_Compiler.m_IRBuilder.CreateLoad(m_LastVisitedValue, "var"), decl);
			}
		}

		return;
	}

	if (const auto enumeratorDecl = decl.Cast<Declaration::EnumConstantDecl>())
	{
		if (m_RequiredModifiableValue)
		{
			nat_Throw(AotCompilerException, u8"此定义不可变"_nv);
		}

		setLastVisitedResult(llvm::ConstantInt::get(m_Compiler.getCorrespondingType(enumeratorDecl->GetValueType()), static_cast<std::uint64_t>(enumeratorDecl->GetValue())), decl);
		return;
	}

	nat_Throw(AotCompilerException, u8"定义引用表达式引用了不存在或不合法的定义"_nv);
}

void AotCompiler::AotStmtVisitor::VisitFloatingLiteral(natRefPointer<Expression::FloatingLiteral> const& expr)
{
	const auto floatType = expr->GetExprType().Cast<Type::BuiltinType>();

	if (floatType->GetBuiltinClass() == Type::BuiltinType::Float)
	{
		setLastVisitedResult(llvm::ConstantFP::get(m_Compiler.m_LLVMContext, llvm::APFloat{ static_cast<nFloat>(expr->GetValue()) }));
	}
	else
	{
		setLastVisitedResult(llvm::ConstantFP::get(m_Compiler.m_LLVMContext, llvm::APFloat{ expr->GetValue() }));
	}
}

void AotCompiler::AotStmtVisitor::VisitIntegerLiteral(natRefPointer<Expression::IntegerLiteral> const& expr)
{
	if (m_RequiredModifiableValue)
	{
		nat_Throw(AotCompilerException, u8"当前表达式无法求值为可修改的值"_nv);
	}

	const auto intType = expr->GetExprType().Cast<Type::BuiltinType>();
	const auto typeInfo = m_Compiler.m_AstContext.GetTypeInfo(intType);

	setLastVisitedResult(llvm::ConstantInt::get(m_Compiler.m_LLVMContext, llvm::APInt{ static_cast<unsigned>(typeInfo.Size * 8), expr->GetValue(), intType->IsSigned() }));
}

void AotCompiler::AotStmtVisitor::VisitMemberExpr(natRefPointer<Expression::MemberExpr> const& expr)
{
	const auto memberDecl = expr->GetMemberDecl();

	if (const auto method = memberDecl.Cast<Declaration::MethodDecl>())
	{
		if (m_RequiredModifiableValue)
		{
			nat_Throw(AotCompilerException, u8"当前表达式无法求值为可修改的值"_nv);
		}

		const auto iter = m_Compiler.m_FunctionMap.find(method);
		if (iter == m_Compiler.m_FunctionMap.end())
		{
			nat_Throw(AotCompilerException, u8"引用了不存在的成员"_nv);
		}

		setLastVisitedResult(iter->second, memberDecl);
	}
	else if (const auto field = memberDecl.Cast<Declaration::FieldDecl>())
	{
		const auto baseExpr = expr->GetBase();
		EvaluateAsModifiableValue(baseExpr);
		auto baseValue = m_LastVisitedValue;
		auto baseType = baseExpr->GetExprType();

		if (const auto pointerType = baseType.Cast<Type::PointerType>())
		{
			baseType = pointerType->GetPointeeType();
			baseValue = m_Compiler.m_IRBuilder.CreateLoad(baseValue);
		}

		natRefPointer<Declaration::ClassDecl> baseClass;
		if (const auto classType = baseType.Cast<Type::ClassType>())
		{
			baseClass = classType->GetDecl();
		}

		assert(baseClass);

		const auto& classLayout = m_Compiler.m_AstContext.GetClassLayout(baseClass);
		const auto fieldInfo = classLayout.GetFieldInfo(field);
		if (!fieldInfo)
		{
			nat_Throw(AotCompilerException, u8"引用了不存在的成员"_nv);
		}

		const auto fieldIndex = fieldInfo.value().first;
		const auto fieldName = field->GetName();
		const auto memberPtr = m_Compiler.m_IRBuilder.CreateGEP(baseValue,
			{ llvm::ConstantInt::getNullValue(llvm::Type::getInt64Ty(m_Compiler.m_LLVMContext)), llvm::ConstantInt::get(llvm::Type::getInt32Ty(m_Compiler.m_LLVMContext), fieldIndex) },
			llvm::StringRef{ fieldName.data(), fieldName.size() });
		setLastVisitedResult(m_RequiredModifiableValue ? memberPtr : m_Compiler.m_IRBuilder.CreateLoad(memberPtr, llvm::StringRef{ fieldName.data(), fieldName.size() }), memberDecl);
	}
	else
	{
		nat_Throw(AotCompilerException, u8"引用了错误的成员，前端可能出现 bug"_nv);
	}
}

void AotCompiler::AotStmtVisitor::VisitParenExpr(natRefPointer<Expression::ParenExpr> const& expr)
{
	EvaluateValue(expr->GetInnerExpr());
}

void AotCompiler::AotStmtVisitor::VisitStmtExpr(natRefPointer<Expression::StmtExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitStringLiteral(natRefPointer<Expression::StringLiteral> const& expr)
{
	setLastVisitedResult(m_Compiler.getStringLiteralValue(expr->GetValue()));
}

void AotCompiler::AotStmtVisitor::VisitUnaryOperator(natRefPointer<Expression::UnaryOperator> const& expr)
{
	const auto operand = expr->GetOperand();
	const auto opType = operand->GetExprType();
	const auto opCode = expr->GetOpcode();

	switch (opCode)
	{
	case Expression::UnaryOperationType::PostInc:
		EvaluateAsModifiableValue(operand);
		setLastVisitedResult(EmitIncDec(m_LastVisitedValue, opType, true, false));
		break;
	case Expression::UnaryOperationType::PreInc:
		EvaluateAsModifiableValue(operand);
		setLastVisitedResult(EmitIncDec(m_LastVisitedValue, opType, true, true));
		break;
	case Expression::UnaryOperationType::PostDec:
		EvaluateAsModifiableValue(operand);
		setLastVisitedResult(EmitIncDec(m_LastVisitedValue, opType, false, false));
		break;
	case Expression::UnaryOperationType::PreDec:
		EvaluateAsModifiableValue(operand);
		setLastVisitedResult(EmitIncDec(m_LastVisitedValue, opType, false, true));
		break;
	case Expression::UnaryOperationType::AddrOf:
		// 返回值即为地址
		if (operand->GetExprType().Cast<Type::FunctionType>())
		{
			EvaluateValue(operand);
		}
		else
		{
			EvaluateAsModifiableValue(operand);
		}

		break;
	case Expression::UnaryOperationType::Deref:
		EvaluateValue(operand);
		if (!m_RequiredModifiableValue)
		{
			setLastVisitedResult(m_Compiler.m_IRBuilder.CreateLoad(m_LastVisitedValue, "deref"));
		}

		break;
	case Expression::UnaryOperationType::Plus:
		EvaluateValue(operand);
		break;
	case Expression::UnaryOperationType::Minus:
		EvaluateValue(operand);
		if (const auto builtinType = Type::Type::GetUnderlyingType(operand->GetExprType()).Cast<Type::BuiltinType>())
		{
			if (builtinType->IsIntegerType())
			{
				setLastVisitedResult(m_Compiler.m_IRBuilder.CreateNeg(m_LastVisitedValue));
			}
			else
			{
				assert(builtinType->IsFloatingType());
				setLastVisitedResult(m_Compiler.m_IRBuilder.CreateFNeg(m_LastVisitedValue));
			}
		}
		else
		{
			nat_Throw(NotImplementedException);
		}

		break;
	case Expression::UnaryOperationType::Not:
		EvaluateValue(operand);
		setLastVisitedResult(m_Compiler.m_IRBuilder.CreateNot(m_LastVisitedValue));
		break;
	case Expression::UnaryOperationType::LNot:
		EvaluateAsBool(operand);
		setLastVisitedResult(m_Compiler.m_IRBuilder.CreateIsNull(m_LastVisitedValue));
		break;
	default:
		assert(!"Invalid opCode");
		[[fallthrough]];
	case Expression::UnaryOperationType::Invalid:
		nat_Throw(AotCompilerException, u8"错误的操作码"_nv);
	}
}

void AotCompiler::AotStmtVisitor::VisitForStmt(natRefPointer<Statement::ForStmt> const& stmt)
{
	const auto forEnd = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "for.end");

	{
		LexicalScope forScope{ *this, { stmt->GetStartLoc(), stmt->GetEndLoc() } };

		if (const auto init = stmt->GetInit())
		{
			Visit(init);
		}

		const auto forContinue = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "for.cond");
		auto forInc = forContinue;
		EmitBlock(forContinue);

		const auto inc = stmt->GetInc();
		if (inc)
		{
			forInc = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "for.inc");
		}

		m_BreakContinueStack.emplace_back(JumpDest{ forEnd, GetCleanupStackTop(), true }, JumpDest{ forInc, GetCleanupStackTop(), inc });

		if (const auto cond = stmt->GetCond())
		{
			const auto forBody = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "for.body");

			EvaluateAsBool(cond);
			const auto condValue = m_LastVisitedValue;

			m_Compiler.m_IRBuilder.CreateCondBr(condValue, forBody, forEnd);

			EmitBlock(forBody);
		}

		{
			const auto forBodyStmt = stmt->GetBody();
			LexicalScope scope{ *this, { forBodyStmt->GetStartLoc(), forBodyStmt->GetEndLoc() } };
			if (forBodyStmt->GetType() == Statement::Stmt::CompoundStmtClass)
			{
				scope.SetAlreadyCleaned();
			}
			Visit(forBodyStmt);
		}

		m_BreakContinueStack.pop_back();

		if (inc)
		{
			EmitBlock(forInc);
			Visit(inc);
		}

		EmitBranch(forContinue);
	}

	EmitBlock(forEnd);
}

void AotCompiler::AotStmtVisitor::VisitGotoStmt(natRefPointer<Statement::GotoStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitIfStmt(natRefPointer<Statement::IfStmt> const& stmt)
{
	EvaluateAsBool(stmt->GetCond());
	const auto condExpr = m_LastVisitedValue;

	const auto thenStmt = stmt->GetThen();
	const auto elseStmt = stmt->GetElse();

	const auto trueBranch = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "if.then");
	const auto endBranch = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "if.end");
	const auto falseBranch = elseStmt ? llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "if.else") : endBranch;

	m_Compiler.m_IRBuilder.CreateCondBr(condExpr, trueBranch, falseBranch);

	EmitBlock(trueBranch);
	{
		LexicalScope scope{ *this, { stmt->GetStartLoc(), stmt->GetEndLoc() } };
		if (thenStmt->GetType() == Statement::Stmt::CompoundStmtClass)
		{
			scope.SetAlreadyCleaned();
		}
		Visit(thenStmt);
	}
	EmitBranch(endBranch);

	if (elseStmt)
	{
		EmitBlock(falseBranch);
		{
			LexicalScope scope{ *this, { stmt->GetStartLoc(), stmt->GetEndLoc() } };
			if (elseStmt->GetType() == Statement::Stmt::CompoundStmtClass)
			{
				scope.SetAlreadyCleaned();
			}
			Visit(elseStmt);
		}
		EmitBranch(endBranch);
	}

	EmitBlock(endBranch, true);
}

void AotCompiler::AotStmtVisitor::VisitLabelStmt(natRefPointer<Statement::LabelStmt> const& stmt)
{
	// TODO: 添加标签
	Visit(stmt->GetSubStmt());
}

void AotCompiler::AotStmtVisitor::VisitNullStmt(natRefPointer<Statement::NullStmt> const& /*stmt*/)
{
}

// TODO: 生成清理代码
void AotCompiler::AotStmtVisitor::VisitReturnStmt(natRefPointer<Statement::ReturnStmt> const& stmt)
{
	if (const auto retExpr = stmt->GetReturnExpr())
	{
		EvaluateValue(retExpr);
		m_Compiler.m_IRBuilder.CreateStore(m_LastVisitedValue, m_ReturnValue);
	}

	EmitBranchWithCleanup(m_ReturnBlock);
}

void AotCompiler::AotStmtVisitor::VisitSwitchCase(natRefPointer<Statement::SwitchCase> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCaseStmt(natRefPointer<Statement::CaseStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitDefaultStmt(natRefPointer<Statement::DefaultStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitSwitchStmt(natRefPointer<Statement::SwitchStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitWhileStmt(natRefPointer<Statement::WhileStmt> const& stmt)
{
	const auto loopHead = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "while.cond");
	EmitBlock(loopHead);

	const auto loopEnd = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "while.end");

	m_BreakContinueStack.emplace_back(JumpDest{ loopEnd, GetCleanupStackTop(), true }, JumpDest{ loopHead, GetCleanupStackTop(), true });

	EvaluateAsBool(stmt->GetCond());
	const auto cond = m_LastVisitedValue;

	// while (true) {} 较为常用，针对这个场景优化
	auto alwaysTrue = false;
	if (const auto val = llvm::dyn_cast<llvm::Constant>(cond))
	{
		if (!val->isNullValue())
		{
			alwaysTrue = true;
		}
	}

	const auto loopBody = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "while.body");
	if (!alwaysTrue)
	{
		m_Compiler.m_IRBuilder.CreateCondBr(cond, loopBody, loopEnd);
	}

	EmitBlock(loopBody);
	{
		const auto body = stmt->GetBody();
		LexicalScope scope{ *this, { body->GetStartLoc(), body->GetEndLoc() } };
		if (body->GetType() == Statement::Stmt::CompoundStmtClass)
		{
			scope.SetAlreadyCleaned();
		}
		Visit(body);
	}

	m_BreakContinueStack.pop_back();

	EmitBranch(loopHead);

	EmitBlock(loopEnd);
}

void AotCompiler::AotStmtVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitNullPointerLiteral(natRefPointer<Expression::NullPointerLiteral> const& expr)
{
	setLastVisitedResult(llvm::Constant::getNullValue(llvm::PointerType::get(llvm::IntegerType::getInt8Ty(m_Compiler.m_LLVMContext), 0)));
}

void AotCompiler::AotStmtVisitor::StartVisit()
{
	const auto body = m_CurrentFunction->GetBody();
	assert(body);

	if (const auto compoundStmt = body.Cast<Statement::CompoundStmt>())
	{
		EmitCompoundStmtWithoutScope(compoundStmt);
	}
	else
	{
		Visit(body);
	}
}

void AotCompiler::AotStmtVisitor::EndVisit()
{
	EmitBlock(m_ReturnBlock.GetBlock());
	EmitFunctionEpilog();
}

llvm::Function* AotCompiler::AotStmtVisitor::GetFunction() const
{
	std::string verifyInfo;
	llvm::raw_string_ostream os{ verifyInfo };
	if (verifyFunction(*m_CurrentFunctionValue, &os))
	{
		nat_Throw(AotCompilerException, u8"函数验证错误，信息为 {0}"_nv, verifyInfo);
	}

	return m_CurrentFunctionValue;
}

void AotCompiler::AotStmtVisitor::EmitFunctionEpilog()
{
	PopCleanupStack(m_CleanupStack.end());

	if (!m_ReturnValue)
	{
		assert(m_CurrentFunction->GetValueType().Cast<Type::FunctionType>()->GetResultType()->IsVoid());
		m_Compiler.m_IRBuilder.CreateRetVoid();
		return;
	}

	const auto retValue = m_Compiler.m_IRBuilder.CreateLoad(m_ReturnValue, "retValue");
	m_Compiler.m_IRBuilder.CreateRet(retValue);
}

void AotCompiler::AotStmtVisitor::EmitAddressOfVar(natRefPointer<Declaration::VarDecl> const& varDecl)
{
	assert(varDecl);

	const auto localDeclIter = m_DeclMap.find(varDecl);
	if (localDeclIter != m_DeclMap.end())
	{
		setLastVisitedResult(localDeclIter->second);
		return;
	}

	const auto nonLocalDeclIter = m_Compiler.m_GlobalVariableMap.find(varDecl);
	if (nonLocalDeclIter != m_Compiler.m_GlobalVariableMap.end())
	{
		setLastVisitedResult(nonLocalDeclIter->second);
	}
}

void AotCompiler::AotStmtVisitor::EmitCompoundStmt(natRefPointer<Statement::CompoundStmt> const& compoundStmt)
{
	LexicalScope scope{ *this, { compoundStmt->GetStartLoc(), compoundStmt->GetEndLoc() } };

	EmitCompoundStmtWithoutScope(compoundStmt);
}

void AotCompiler::AotStmtVisitor::EmitCompoundStmtWithoutScope(natRefPointer<Statement::CompoundStmt> const& compoundStmt)
{
	for (auto&& item : compoundStmt->GetChildrenStmt())
	{
		Visit(item);
	}
}

void AotCompiler::AotStmtVisitor::EmitBranch(llvm::BasicBlock* target)
{
	const auto curBlock = m_Compiler.m_IRBuilder.GetInsertBlock();

	if (curBlock && !curBlock->getTerminator())
	{
		m_Compiler.m_IRBuilder.CreateBr(target);
	}

	m_Compiler.m_IRBuilder.ClearInsertionPoint();
}

void AotCompiler::AotStmtVisitor::EmitBranchWithCleanup(JumpDest const& target)
{
	const auto targetCleanupIterator = target.GetCleanupIterator();
	assert(CleanupEncloses(targetCleanupIterator, GetCleanupStackTop()));

	if (!m_Compiler.m_IRBuilder.GetInsertBlock())
	{
		m_Compiler.m_IRBuilder.ClearInsertionPoint();
		return;
	}

	auto block = target.GetBlock();
	const auto scope = LookupLexicalScopeAfter(targetCleanupIterator);
	if (scope)
	{
		const auto curScope = m_CurrentLexicalScope;
		if (curScope)
		{
			curScope->ExplicitClean();
		}

		PopCleanupStack(scope->GetBeginIterator(), false);

		if (target.IsAfterLexicalScope())
		{
			block = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "cleanup");
			curScope->EnsureEncloses(InsertCleanupStack(scope->GetBeginIterator(), make_ref<AnchorCleanup>(block)));
		}
	}

	m_Compiler.m_IRBuilder.CreateBr(block);
	m_Compiler.m_IRBuilder.ClearInsertionPoint();
}

void AotCompiler::AotStmtVisitor::EmitBlock(llvm::BasicBlock* block, nBool finished)
{
	const auto curBlock = m_Compiler.m_IRBuilder.GetInsertBlock();

	EmitBranch(block);

	if (finished && block->use_empty())
	{
		delete block;
		return;
	}

	if (curBlock && curBlock->getParent())
	{
		m_CurrentFunctionValue->getBasicBlockList().insertAfter(curBlock->getIterator(), block);
	}
	else
	{
		m_CurrentFunctionValue->getBasicBlockList().push_back(block);
	}

	m_Compiler.m_IRBuilder.SetInsertPoint(block);
}

llvm::Value* AotCompiler::AotStmtVisitor::EmitBinOp(llvm::Value* leftOperand, llvm::Value* rightOperand,
	Expression::BinaryOperationType opCode, Type::TypePtr const& leftType, Type::TypePtr const& rightType,
	Type::TypePtr const& resultType)
{
	switch (leftType->GetType())
	{
	case Type::Type::Builtin:
		switch (rightType->GetType())
		{
		case Type::Type::Builtin:
			assert(leftType == rightType);
			return EmitBuiltinBinOp(leftOperand, rightOperand, opCode, leftType, resultType);
		case Type::Type::Pointer:
			switch (opCode)
			{
			case Expression::BinaryOperationType::Add:
				return m_Compiler.m_IRBuilder.CreateInBoundsGEP(rightOperand, leftOperand, "ptradd");
			default:
				assert(!"Invalid opCode");
				break;
			}
			break;
		case Type::Type::Array:
			break;
		case Type::Type::Function:
			break;
		case Type::Type::Class:
			break;
		case Type::Type::Enum:
			break;
		default:
			assert(!"Invalid type");
			break;
		}
		break;
	case Type::Type::Pointer:
		switch (rightType->GetType())
		{
		case Type::Type::Builtin:
			switch (opCode)
			{
			case Expression::BinaryOperationType::Add:
				return m_Compiler.m_IRBuilder.CreateInBoundsGEP(leftOperand, rightOperand, "addptr");
			case Expression::BinaryOperationType::Sub:
				return m_Compiler.m_IRBuilder.CreateInBoundsGEP(leftOperand, m_Compiler.m_IRBuilder.CreateNeg(rightOperand, "diff"), "subptr");
			default:
				assert(!"Invalid opCode");
				break;
			}
			break;
		case Type::Type::Pointer:
		{
			const auto ptrDiff = m_Compiler.m_IRBuilder.CreatePtrDiff(leftOperand, rightOperand, "ptrdiff");
			const auto ptrDiffType = m_Compiler.getCorrespondingType(m_Compiler.m_AstContext.GetPtrDiffType());

			switch (opCode)
			{
			case Expression::BinaryOperationType::Sub:
				return ptrDiff;
			case Expression::BinaryOperationType::LT:
				return m_Compiler.m_IRBuilder.CreateICmpSLT(ptrDiff, llvm::Constant::getNullValue(ptrDiffType), "cmp");
			case Expression::BinaryOperationType::GT:
				return m_Compiler.m_IRBuilder.CreateICmpSGT(ptrDiff, llvm::Constant::getNullValue(ptrDiffType), "cmp");
			case Expression::BinaryOperationType::LE:
				return m_Compiler.m_IRBuilder.CreateICmpSLE(ptrDiff, llvm::Constant::getNullValue(ptrDiffType), "cmp");
			case Expression::BinaryOperationType::GE:
				return m_Compiler.m_IRBuilder.CreateICmpSGE(ptrDiff, llvm::Constant::getNullValue(ptrDiffType), "cmp");
			case Expression::BinaryOperationType::EQ:
				return m_Compiler.m_IRBuilder.CreateIsNull(ptrDiff, "cmp");
			case Expression::BinaryOperationType::NE:
				return m_Compiler.m_IRBuilder.CreateIsNotNull(ptrDiff, "cmp");
			default:
				assert(!"Invalid opCode");
				break;
			}
			break;
		}
		case Type::Type::Array:
			break;
		case Type::Type::Class:
			break;
		case Type::Type::Enum:
			break;
		default:
			break;
		}
		break;
	case Type::Type::Array:
		break;
	case Type::Type::Function:
		break;
	case Type::Type::Class:
		break;
	case Type::Type::Enum:
		switch (rightType->GetType())
		{
		case Type::Type::Builtin:
			break;
		case Type::Type::Pointer:
			break;
		case Type::Type::Array:
			break;
		case Type::Type::Function:
			break;
		case Type::Type::Class:
			break;
		case Type::Type::Enum:
			assert(leftType == rightType);
			return EmitBuiltinBinOp(leftOperand, rightOperand, opCode, leftType.UnsafeCast<Type::EnumType>()->GetDecl().UnsafeCast<Declaration::EnumDecl>()->GetUnderlyingType(), resultType);
		default:
			break;
		}
		break;
	default:
		assert(!"Invalid type");
		break;
	}

	nat_Throw(NotImplementedException, u8"对操作数 {0} 及 {1} 的 {2} 操作未实现或未定义"_nv, m_Compiler.m_Sema.GetTypeName(leftType), m_Compiler.m_Sema.GetTypeName(rightType), Expression::GetBinaryOperationTypeName(opCode));
}

llvm::Value* AotCompiler::AotStmtVisitor::EmitBuiltinBinOp(llvm::Value* leftOperand, llvm::Value* rightOperand, Expression::BinaryOperationType opCode, natRefPointer<Type::BuiltinType> const& commonType, natRefPointer<Type::BuiltinType> const& resultType)
{
	switch (opCode)
	{
	case Expression::BinaryOperationType::Mul:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFMul(leftOperand, rightOperand, "fmul");
		}

		return m_Compiler.m_IRBuilder.CreateMul(leftOperand, rightOperand, "mul");
	case Expression::BinaryOperationType::Div:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFDiv(leftOperand, rightOperand, "fdiv");
		}

		if (commonType->IsSigned())
		{
			return m_Compiler.m_IRBuilder.CreateSDiv(leftOperand, rightOperand, "div");
		}

		return m_Compiler.m_IRBuilder.CreateUDiv(leftOperand, rightOperand, "div");
	case Expression::BinaryOperationType::Rem:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFRem(leftOperand, rightOperand, "fmod");
		}

		if (commonType->IsSigned())
		{
			return m_Compiler.m_IRBuilder.CreateSRem(leftOperand, rightOperand, "mod");
		}

		return m_Compiler.m_IRBuilder.CreateURem(leftOperand, rightOperand, "mod");
	case Expression::BinaryOperationType::Add:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFAdd(leftOperand, rightOperand, "fadd");
		}

		return m_Compiler.m_IRBuilder.CreateAdd(leftOperand, rightOperand, "add");
	case Expression::BinaryOperationType::Sub:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFSub(leftOperand, rightOperand, "fsub");
		}

		return m_Compiler.m_IRBuilder.CreateSub(leftOperand, rightOperand, "sub");
	case Expression::BinaryOperationType::Shl:
		return m_Compiler.m_IRBuilder.CreateShl(leftOperand, rightOperand, "shl");
	case Expression::BinaryOperationType::Shr:
		if (commonType->IsSigned())
		{
			return m_Compiler.m_IRBuilder.CreateAShr(leftOperand, rightOperand, "shr");
		}

		return m_Compiler.m_IRBuilder.CreateLShr(leftOperand, rightOperand, "shr");
	case Expression::BinaryOperationType::LT:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFCmpOLT(leftOperand, rightOperand, "cmp");
		}

		if (commonType->IsSigned())
		{
			return m_Compiler.m_IRBuilder.CreateICmpSLT(leftOperand, rightOperand, "cmp");
		}

		return m_Compiler.m_IRBuilder.CreateICmpULT(leftOperand, rightOperand, "cmp");
	case Expression::BinaryOperationType::GT:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFCmpOGT(leftOperand, rightOperand, "cmp");
		}

		if (commonType->IsSigned())
		{
			return m_Compiler.m_IRBuilder.CreateICmpSGT(leftOperand, rightOperand, "cmp");
		}

		return m_Compiler.m_IRBuilder.CreateICmpUGT(leftOperand, rightOperand, "cmp");
	case Expression::BinaryOperationType::LE:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFCmpOLE(leftOperand, rightOperand, "cmp");
		}

		if (commonType->IsSigned())
		{
			return m_Compiler.m_IRBuilder.CreateICmpSLE(leftOperand, rightOperand, "cmp");
		}

		return m_Compiler.m_IRBuilder.CreateICmpULE(leftOperand, rightOperand, "cmp");
	case Expression::BinaryOperationType::GE:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFCmpOGE(leftOperand, rightOperand, "cmp");
		}

		if (commonType->IsSigned())
		{
			return m_Compiler.m_IRBuilder.CreateICmpSGE(leftOperand, rightOperand, "cmp");
		}

		return m_Compiler.m_IRBuilder.CreateICmpUGE(leftOperand, rightOperand, "cmp");
	case Expression::BinaryOperationType::EQ:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFCmpOEQ(leftOperand, rightOperand, "cmp");
		}

		return m_Compiler.m_IRBuilder.CreateICmpEQ(leftOperand, rightOperand, "cmp");
	case Expression::BinaryOperationType::NE:
		if (commonType->IsFloatingType())
		{
			return m_Compiler.m_IRBuilder.CreateFCmpUNE(leftOperand, rightOperand, "cmp");
		}

		return m_Compiler.m_IRBuilder.CreateICmpNE(leftOperand, rightOperand, "cmp");
	case Expression::BinaryOperationType::And:
		return m_Compiler.m_IRBuilder.CreateAnd(leftOperand, rightOperand, "and");
	case Expression::BinaryOperationType::Xor:
		return m_Compiler.m_IRBuilder.CreateXor(leftOperand, rightOperand, "xor");
	case Expression::BinaryOperationType::Or:
		return m_Compiler.m_IRBuilder.CreateOr(leftOperand, rightOperand, "or");
	case Expression::BinaryOperationType::LAnd:
	{
		auto rhsBlock = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "land.rhs");
		const auto endBlock = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "land.end");

		m_Compiler.m_IRBuilder.CreateCondBr(leftOperand, rhsBlock, endBlock);

		const auto phiNode = llvm::PHINode::Create(llvm::Type::getInt1Ty(m_Compiler.m_LLVMContext), 2, "", endBlock);

		for (auto i = llvm::pred_begin(endBlock), end = llvm::pred_end(endBlock); i != end; ++i)
		{
			phiNode->addIncoming(llvm::ConstantInt::getFalse(m_Compiler.m_LLVMContext), *i);
		}

		EmitBlock(rhsBlock);
		const auto rhsCond = ConvertScalarToBool(rightOperand, m_Compiler.m_AstContext.GetBuiltinType(Type::BuiltinType::Bool));

		rhsBlock = m_Compiler.m_IRBuilder.GetInsertBlock();

		EmitBlock(endBlock);
		phiNode->addIncoming(rhsCond, rhsBlock);

		return m_Compiler.m_IRBuilder.CreateZExtOrBitCast(phiNode, m_Compiler.getCorrespondingType(resultType));
	}
	case Expression::BinaryOperationType::LOr:
	{
		auto rhsBlock = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "lor.rhs");
		const auto endBlock = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "lor.end");

		m_Compiler.m_IRBuilder.CreateCondBr(leftOperand, endBlock, rhsBlock);

		const auto phiNode = llvm::PHINode::Create(llvm::Type::getInt1Ty(m_Compiler.m_LLVMContext), 2, "", endBlock);

		for (auto i = llvm::pred_begin(endBlock), end = llvm::pred_end(endBlock); i != end; ++i)
		{
			phiNode->addIncoming(llvm::ConstantInt::getTrue(m_Compiler.m_LLVMContext), *i);
		}

		EmitBlock(rhsBlock);
		const auto rhsCond = ConvertScalarToBool(rightOperand, m_Compiler.m_AstContext.GetBuiltinType(Type::BuiltinType::Bool));

		rhsBlock = m_Compiler.m_IRBuilder.GetInsertBlock();

		EmitBlock(endBlock);
		phiNode->addIncoming(rhsCond, rhsBlock);

		return m_Compiler.m_IRBuilder.CreateZExtOrBitCast(phiNode, m_Compiler.getCorrespondingType(resultType));
	}
	default:
		assert(!"Invalid Opcode");
		nat_Throw(AotCompilerException, u8"无效的 Opcode"_nv);
	}
}

llvm::Value* AotCompiler::AotStmtVisitor::EmitBuiltinIncDec(llvm::Value* operand, natRefPointer<Type::BuiltinType> const& opType, nBool isInc, nBool isPre)
{
	llvm::Value* oldValue = m_Compiler.m_IRBuilder.CreateLoad(operand);
	llvm::Value* value;

	if (opType->IsFloatingType())
	{
		value = m_Compiler.m_IRBuilder.CreateFAdd(oldValue, llvm::ConstantFP::get(m_Compiler.getCorrespondingType(opType), isInc ? 1.0 : -1.0));
	}
	else
	{
		value = m_Compiler.m_IRBuilder.CreateAdd(oldValue, llvm::ConstantInt::get(m_Compiler.getCorrespondingType(opType), isInc ? 1 : -1, opType->IsSigned()));
	}

	m_Compiler.m_IRBuilder.CreateStore(value, operand);

	if (m_RequiredModifiableValue)
	{
		if (isPre)
		{
			return operand;
		}

		nat_Throw(AotCompilerException, u8"当前表达式无法求值为可修改的值"_nv);
	}

	if (isPre)
	{
		return value;
	}

	return oldValue;
}

llvm::Value* AotCompiler::AotStmtVisitor::EmitIncDec(llvm::Value* operand, const Type::TypePtr& opType, nBool isInc, nBool isPre)
{
	const auto realType = Type::Type::GetUnderlyingType(opType);
	switch (realType->GetType())
	{
	case Type::Type::Builtin:
		return EmitBuiltinIncDec(operand, realType.UnsafeCast<Type::BuiltinType>(), isInc, isPre);
	case Type::Type::Pointer:
	{
		const auto oldValue = m_Compiler.m_IRBuilder.CreateLoad(operand);
		const auto value = m_Compiler.m_IRBuilder.CreateInBoundsGEP(oldValue,
			{ llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(m_Compiler.m_LLVMContext), isInc ? 1 : -1, true) }, "addptr");

		m_Compiler.m_IRBuilder.CreateStore(value, operand);

		if (m_RequiredModifiableValue)
		{
			if (isPre)
			{
				return operand;
			}

			nat_Throw(AotCompilerException, u8"当前表达式无法求值为可修改的值"_nv);
		}

		if (isPre)
		{
			return value;
		}

		return oldValue;
	}
	case Type::Type::Array:
		break;
	case Type::Type::Function:
		break;
	case Type::Type::Class:
		break;
	case Type::Type::Enum:
		break;
	default:
		break;
	}

	nat_Throw(NotImplementedException);
}

llvm::Value* AotCompiler::AotStmtVisitor::EmitFunctionAddr(natRefPointer<Declaration::FunctionDecl> const& func)
{
	assert(func);

	if (!m_RequiredModifiableValue)
	{
		const auto funcIter = m_Compiler.m_FunctionMap.find(func);
		if (funcIter != m_Compiler.m_FunctionMap.end())
		{
			setLastVisitedResult(funcIter->second);
			return m_LastVisitedValue;
		}

		nat_Throw(AotCompilerException, u8"无法找到函数 \"{0}\""_nv, GetQualifiedName(func, false));
	}

	nat_Throw(AotCompilerException, u8"无法修改函数"_nv);
}

void AotCompiler::AotStmtVisitor::EmitVarDecl(natRefPointer<Declaration::VarDecl> const& decl)
{
	switch (decl->GetStorageClass())
	{
	default:
		assert(!"Invalid storage class");
		[[fallthrough]];
	case Specifier::StorageClass::None:
		return EmitAutoVarDecl(decl);
	case Specifier::StorageClass::Extern:
		return EmitExternVarDecl(decl);
	case Specifier::StorageClass::Static:
		return EmitStaticVarDecl(decl);
	}
}

void AotCompiler::AotStmtVisitor::EmitAutoVarDecl(natRefPointer<Declaration::VarDecl> const& decl)
{
	const auto type = decl->GetValueType();
	const auto storage = EmitAutoVarAlloc(decl);
	EmitAutoVarInit(type, storage, decl->GetInitializer());
	EmitAutoVarCleanup(type, storage);
}

llvm::Value* AotCompiler::AotStmtVisitor::EmitAutoVarAlloc(natRefPointer<Declaration::VarDecl> const& decl)
{
	const auto type = decl->GetValueType();

	// 可能用于动态大小的类型
	llvm::Value* arraySize = nullptr;

	const auto varName = decl->GetName();
	const auto valueType = m_Compiler.getCorrespondingType(type);
	const auto typeInfo = m_Compiler.m_AstContext.GetTypeInfo(type);

	const auto storage = m_Compiler.m_IRBuilder.CreateAlloca(valueType, arraySize, std::string(varName.cbegin(), varName.cend()));
	storage->setAlignment(static_cast<unsigned>(typeInfo.Align));

	m_DeclMap.emplace(decl, storage);

	return storage;
}

void AotCompiler::AotStmtVisitor::EmitAutoVarInit(Type::TypePtr const& varType, llvm::Value* varPtr, Expression::ExprPtr const& initializer)
{
	const auto valueType = m_Compiler.getCorrespondingType(varType);
	const auto typeInfo = m_Compiler.m_AstContext.GetTypeInfo(varType);

	if (initializer)
	{
		if (const auto initListExpr = initializer.Cast<Expression::InitListExpr>())
		{
			if (const auto arrayType = varType.Cast<Type::ArrayType>())
			{
				const auto initExprs = initListExpr->GetInitExprs().Cast<std::vector<Expression::ExprPtr>>();

				// TODO: 需要更好的方案
				if (initExprs.size() < arrayType->GetSize())
				{
					m_Compiler.m_IRBuilder.CreateMemSet(varPtr, llvm::ConstantInt::get(llvm::IntegerType::getInt8Ty(m_Compiler.m_LLVMContext), 0), typeInfo.Size, static_cast<unsigned>(typeInfo.Align));
				}

				for (std::size_t i = 0; i < initExprs.size(); ++i)
				{
					const auto initExpr = initExprs[i];
					const auto elemPtr = m_Compiler.m_IRBuilder.CreateGEP(valueType, varPtr,
						{ llvm::ConstantInt::get(llvm::Type::getInt64Ty(m_Compiler.m_LLVMContext), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(m_Compiler.m_LLVMContext), i) });

					EmitAutoVarInit(arrayType->GetElementType(), elemPtr, initExpr);
				}
			}
			else if (varType->GetType() == Type::Type::Builtin || varType->GetType() == Type::Type::Pointer)
			{
				const auto initExprCount = initListExpr->GetInitExprCount();

				if (initExprCount == 0)
				{
					m_Compiler.m_IRBuilder.CreateStore(llvm::Constant::getNullValue(valueType), varPtr);
				}
				else if (initExprCount == 1)
				{
					const auto initExpr = initListExpr->GetInitExprs().first();
					EvaluateValue(initExpr);
					const auto initializerValue = m_LastVisitedValue;
					m_Compiler.m_IRBuilder.CreateStore(initializerValue, varPtr);
				}
				else
				{
					// TODO: 报告错误
				}
			}
			else
			{
				// TODO: 用户自定义类型初始化
			}
		}
		else if (const auto constructExpr = initializer.Cast<Expression::ConstructExpr>())
		{
			const auto constructorDecl = constructExpr->GetConstructorDecl();

			if (!constructorDecl)
			{
				// FIXME: 应由前端生成默认构造函数
				return;
			}

			const auto iter = m_Compiler.m_FunctionMap.find(constructorDecl);
			if (iter == m_Compiler.m_FunctionMap.end())
			{
				nat_Throw(AotCompilerException, u8"构造表达式引用了不存在的构造函数"_nv);
			}

			const auto constructorValue = iter->second;

			// TODO: 消除重复代码

			// 将要初始化的对象的引用作为第一个参数传入
			std::vector<llvm::Value*> args{ varPtr };
			args.reserve(constructExpr->GetArgCount() + 1);

			// TODO: 实现默认参数
			for (auto&& arg : constructExpr->GetArgs())
			{
				// TODO: 直接按位复制了，在需要的时候应由前端生成复制构造函数，但此处没有看到分配存储？
				EvaluateValue(arg);
				assert(m_LastVisitedValue);
				args.emplace_back(m_LastVisitedValue);
			}

			// TODO: 禁止不在末尾的默认参数及在可变参数之前的默认参数
			if (constructorDecl->GetParamCount() > args.size() - 1)
			{
				for (auto&& param : constructorDecl->GetParams().skip(args.size() - 1))
				{
					const auto defaultArg = param->GetInitializer();
					assert(defaultArg);
					EvaluateValue(defaultArg);
					assert(m_LastVisitedValue);
					args.emplace_back(m_LastVisitedValue);
				}
			}

			// 构造函数返回类型永远是 void
			m_Compiler.m_IRBuilder.CreateCall(constructorValue, args);
		}
		else if (const auto stringLiteral = initializer.Cast<Expression::StringLiteral>())
		{
			const auto arrayType = varType.Cast<Type::ArrayType>();
			const auto literalValue = stringLiteral->GetValue();

			assert(arrayType);
			// 至少要比字面量的大小还大1以存储结尾的0，这将由前端来保证
			assert(arrayType->GetSize() > literalValue.GetSize());

			if (arrayType->GetSize() > literalValue.GetSize() + 1)
			{
				m_Compiler.m_IRBuilder.CreateMemSet(varPtr, llvm::ConstantInt::get(llvm::IntegerType::getInt8Ty(m_Compiler.m_LLVMContext), 0), typeInfo.Size, static_cast<unsigned>(typeInfo.Align));
			}

			const auto stringLiteralPtr = m_Compiler.getStringLiteralValue(literalValue);
			m_Compiler.m_IRBuilder.CreateMemCpy(varPtr, static_cast<unsigned>(typeInfo.Align), stringLiteralPtr, static_cast<unsigned>(typeInfo.Align), literalValue.GetSize() + 1);
		}
		else
		{
			EvaluateValue(initializer);
			const auto initializerValue = m_LastVisitedValue;
			m_Compiler.m_IRBuilder.CreateStore(initializerValue, varPtr);
		}
	}
	else
	{
		m_Compiler.m_IRBuilder.CreateStore(llvm::Constant::getNullValue(valueType), varPtr);
	}
}

void AotCompiler::AotStmtVisitor::EmitAutoVarCleanup(Type::TypePtr const& varType, llvm::Value* varPtr)
{
	if (const auto arrayType = varType.Cast<Type::ArrayType>())
	{
		auto flattenArrayType = m_Compiler.flattenArray(arrayType);

		ArrayCleanup::CleanupFunction func;
		if (const auto classType = flattenArrayType->GetElementType().Cast<Type::ClassType>())
		{
			if (const auto classDecl = classType->GetDecl().UnsafeCast<Declaration::ClassDecl>())
			{
				if (auto destructor = findDestructor(classDecl))
				{
					func = [destructor = std::move(destructor)](AotStmtVisitor& visitor, llvm::Value* addr)
					{
						visitor.EmitDestructorCall(destructor, addr);
					};
				}
			}
		}

		PushCleanupStack(make_ref<ArrayCleanup>(std::move(flattenArrayType), varPtr, std::move(func)));
	}
	else if (const auto classType = varType.Cast<Type::ClassType>())
	{
		const auto classDecl = classType->GetDecl().UnsafeCast<Declaration::ClassDecl>();
		assert(classDecl);

		if (auto destructor = findDestructor(classDecl))
		{
			PushCleanupStack(make_ref<DestructorCleanup>(std::move(destructor), varPtr));
		}
	}
}

void AotCompiler::AotStmtVisitor::EmitExternVarDecl(natRefPointer<Declaration::VarDecl> const& decl)
{
	// TODO
	nat_Throw(NotImplementedException);
}

void AotCompiler::AotStmtVisitor::EmitStaticVarDecl(natRefPointer<Declaration::VarDecl> const& decl)
{
	// TODO
	nat_Throw(NotImplementedException);
}

void AotCompiler::AotStmtVisitor::EmitDestructorCall(natRefPointer<Declaration::DestructorDecl> const& destructor, llvm::Value* addr)
{
	EmitFunctionAddr(destructor);
	const auto func = llvm::dyn_cast<llvm::Function>(m_LastVisitedValue);
	m_Compiler.m_IRBuilder.CreateCall(func, addr);
}

void AotCompiler::AotStmtVisitor::EvaluateValue(Expression::ExprPtr const& expr)
{
	assert(expr);

	const auto scope = make_scope([this, oldValue = std::exchange(m_RequiredModifiableValue, false)]
	{
		m_RequiredModifiableValue = oldValue;
	});

	m_LastVisitedDecl.Reset();
	Visit(expr);
}

void AotCompiler::AotStmtVisitor::EvaluateAsModifiableValue(Expression::ExprPtr const& expr)
{
	assert(expr);

	const auto scope = make_scope([this, oldValue = std::exchange(m_RequiredModifiableValue, true)]
	{
		m_RequiredModifiableValue = oldValue;
	});

	m_LastVisitedDecl.Reset();
	Visit(expr);
}

void AotCompiler::AotStmtVisitor::EvaluateAsBool(Expression::ExprPtr const& expr)
{
	assert(expr);

	EvaluateValue(expr);

	setLastVisitedResult(ConvertScalarToBool(m_LastVisitedValue, expr->GetExprType()));
}

llvm::Value* AotCompiler::AotStmtVisitor::ConvertScalarTo(llvm::Value* from, Type::TypePtr fromType, Type::TypePtr toType)
{
	fromType = Type::Type::GetUnderlyingType(fromType);
	toType = Type::Type::GetUnderlyingType(toType);

Begin:

	if (fromType == toType)
	{
		return from;
	}

	const auto llvmFromType = m_Compiler.getCorrespondingType(fromType), llvmToType = m_Compiler.getCorrespondingType(toType);

	switch (fromType->GetType())
	{
	case Type::Type::Builtin:
	{
		const auto builtinFromType = fromType.UnsafeCast<Type::BuiltinType>();
		switch (toType->GetType())
		{
		case Type::Type::Builtin:
		{
			const auto builtinToType = toType.UnsafeCast<Type::BuiltinType>();
			if (builtinToType->GetBuiltinClass() == Type::BuiltinType::Void)
			{
				return nullptr;
			}

			if (builtinToType->GetBuiltinClass() == Type::BuiltinType::Bool)
			{
				return ConvertScalarToBool(from, builtinFromType);
			}

			if (llvmFromType == llvmToType)
			{
				return from;
			}

			if (builtinFromType->IsIntegerType())
			{
				const auto fromSigned = builtinFromType->IsSigned();

				if (builtinToType->IsIntegerType())
				{
					return m_Compiler.m_IRBuilder.CreateIntCast(from, llvmToType, fromSigned, "scalarconv");
				}

				if (fromSigned)
				{
					return m_Compiler.m_IRBuilder.CreateSIToFP(from, llvmToType, "scalarconv");
				}

				return m_Compiler.m_IRBuilder.CreateUIToFP(from, llvmToType, "scalarconv");
			}

			if (builtinToType->IsIntegerType())
			{
				if (builtinToType->IsSigned())
				{
					return m_Compiler.m_IRBuilder.CreateFPToSI(from, llvmToType, "scalarconv");
				}

				return m_Compiler.m_IRBuilder.CreateFPToUI(from, llvmToType, "scalarconv");
			}

			nInt result;
			const auto succeed = builtinFromType->CompareRankTo(builtinToType, result);
			assert(succeed);

			if (result > 0)
			{
				return m_Compiler.m_IRBuilder.CreateFPTrunc(from, llvmToType, "scalarconv");
			}

			return m_Compiler.m_IRBuilder.CreateFPExt(from, llvmToType, "scalarconv");
		}
		case Type::Type::Pointer:
			if (builtinFromType->GetBuiltinClass() == Type::BuiltinType::Null)
			{
				return llvm::Constant::getNullValue(llvmToType);
			}
			if (builtinFromType->GetBuiltinClass() != Type::BuiltinType::Long)
			{
				// FIXME: 替换成足够长的整数类型，并定义别名
				from = ConvertScalarTo(from, fromType, m_Compiler.m_AstContext.GetBuiltinType(Type::BuiltinType::Long));
			}
			return m_Compiler.m_IRBuilder.CreateIntToPtr(from, llvmToType, "inttoptr");
		case Type::Type::Array: break;
		case Type::Type::Function: break;
		case Type::Type::Class: break;
		case Type::Type::Enum:
			toType = toType.UnsafeCast<Type::EnumType>()->GetDecl().UnsafeCast<Declaration::EnumDecl>()->GetUnderlyingType();
			goto Begin;
		default:
			assert(!"Invalid type");
			nat_Throw(Compiler::AotCompilerException, u8"错误的类型"_nv);
		}
		break;
	}
	case Type::Type::Pointer:
		switch (toType->GetType())
		{
		case Type::Type::Builtin:
		{
			const auto builtinToType = toType.UnsafeCast<Type::BuiltinType>();
			assert(builtinToType->IsIntegerType());
			auto result = m_Compiler.m_IRBuilder.CreatePtrToInt(from, llvm::IntegerType::get(m_Compiler.m_LLVMContext, llvmFromType->getIntegerBitWidth()), "ptrtoint");
			nInt compareResult;
			if (builtinToType->CompareRankTo(Type::BuiltinType::Long, compareResult) && compareResult <= 0)
			{
				if (compareResult < 0)
				{
					result = ConvertScalarTo(result, m_Compiler.m_AstContext.GetBuiltinType(Type::BuiltinType::Long), toType);
				}

				return result;
			}

			nat_Throw(Compiler::AotCompilerException, u8"截断转换的指针的值");
		}
		case Type::Type::Pointer:
		{
			assert(fromType != toType);
			return m_Compiler.m_IRBuilder.CreatePointerBitCastOrAddrSpaceCast(from, llvmToType, "ptrcast");
		}
		case Type::Type::Array: break;
		case Type::Type::Function: break;
		case Type::Type::Class: break;
		case Type::Type::Enum: break;
		default:
			assert(!"Invalid type");
			nat_Throw(Compiler::AotCompilerException, u8"错误的类型"_nv);
		}
		break;
	case Type::Type::Array: break;
	case Type::Type::Function: break;
	case Type::Type::Class: break;
	case Type::Type::Enum:
		fromType = fromType.UnsafeCast<Type::EnumType>()->GetDecl().UnsafeCast<Declaration::EnumDecl>()->GetUnderlyingType();
		goto Begin;
	default:
		assert(!"Invalid type");
		nat_Throw(Compiler::AotCompilerException, u8"错误的类型"_nv);
	}

	// TODO
	nat_Throw(NotImplementedException);
}

llvm::Value* AotCompiler::AotStmtVisitor::ConvertScalarToBool(llvm::Value* from, const Type::TypePtr& fromType)
{
	assert(from && fromType);

	const auto type = Type::Type::GetUnderlyingType(fromType);

	switch (type->GetType())
	{
	case Type::Type::Builtin:
	{
		const auto builtinType = type.UnsafeCast<Type::BuiltinType>();
		if (builtinType->GetBuiltinClass() == Type::BuiltinType::Bool)
		{
			return from;
		}

		if (builtinType->IsFloatingType())
		{
			const auto floatingZero = llvm::ConstantFP::getNullValue(from->getType());
			return m_Compiler.m_IRBuilder.CreateFCmpUNE(from, floatingZero, "floatingtobool");
		}

		assert(builtinType->IsIntegerType());
		return m_Compiler.m_IRBuilder.CreateIsNotNull(from, "inttobool");
	}
	case Type::Type::Pointer:
		return m_Compiler.m_IRBuilder.CreateIsNotNull(from, "ptrtobool");
	case Type::Type::Enum:
		return m_Compiler.m_IRBuilder.CreateIsNotNull(from, "inttobool");
	default:
		// TODO: 由前端检查
		nat_Throw(AotCompilerException, u8"无法对这个类型执行此操作"_nv);
	}
}

AotCompiler::AotStmtVisitor::CleanupIterator AotCompiler::AotStmtVisitor::GetCleanupStackTop() const noexcept
{
	return m_CleanupStack.cbegin();
}

nBool AotCompiler::AotStmtVisitor::IsCleanupStackEmpty() const noexcept
{
	return m_CleanupStack.empty();
}

void AotCompiler::AotStmtVisitor::PushCleanupStack(natRefPointer<ICleanup> cleanup)
{
	m_CleanupStack.emplace_front(std::move(cleanup));
}

AotCompiler::AotStmtVisitor::CleanupIterator AotCompiler::AotStmtVisitor::InsertCleanupStack(CleanupIterator const& pos, natRefPointer<ICleanup> cleanup)
{
	return m_CleanupStack.emplace(pos, std::move(cleanup));
}

void AotCompiler::AotStmtVisitor::PopCleanupStack(CleanupIterator const& iter, nBool popStack)
{
	for (auto i = m_CleanupStack.begin(); i != iter;)
	{
		assert(!m_CleanupStack.empty());
		(*i)->Emit(*this);

		if (popStack)
		{
			i = m_CleanupStack.erase(i);
		}
		else
		{
			++i;
		}
	}
}

// TODO: 想办法改一下
bool AotCompiler::AotStmtVisitor::CleanupEncloses(CleanupIterator const& a, CleanupIterator const& b) const noexcept
{
	if (a == b || a == m_CleanupStack.cend())
	{
		return true;
	}

	return b != m_CleanupStack.cend() && std::distance(m_CleanupStack.cbegin(), a) > std::distance(m_CleanupStack.cbegin(), b);
}

AotCompiler::AotStmtVisitor::LexicalScope* AotCompiler::AotStmtVisitor::LookupLexicalScopeAfter(CleanupIterator const& iter)
{
	auto cur = m_CurrentLexicalScope;
	while (cur && !CleanupEncloses(iter, cur->GetBeginIterator()))
	{
		cur = cur->GetParent();
	}

	return cur;
}

void AotCompiler::AotStmtVisitor::setLastVisitedResult(llvm::Value* lastVisitedValue, Declaration::DeclPtr lastVisitedDecl)
{
	m_LastVisitedValue = lastVisitedValue;
	m_LastVisitedDecl = std::move(lastVisitedDecl);
}

AotCompiler::AotCompiler(natRefPointer<TextReader<StringType::Utf8>> const& diagIdMapFile, natLog& logger)
	: m_TargetTriple{ llvm::sys::getDefaultTargetTriple() }, m_TargetMachine{}, m_IRBuilder{ m_LLVMContext },
	m_DiagConsumer{ make_ref<AotDiagConsumer>(*this) },
	m_Diag{ make_ref<AotDiagIdMap>(diagIdMapFile), m_DiagConsumer },
	m_Logger{ logger },
	m_SourceManager{ m_Diag, m_FileManager },
	m_Preprocessor{ m_Diag, m_SourceManager },
	m_Consumer{ make_ref<AotAstConsumer>(*this) },
	m_Sema{ m_Preprocessor, m_AstContext, m_Consumer },
	m_Parser{ m_Preprocessor, m_Sema }
{
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	std::string error;
	const auto target = llvm::TargetRegistry::lookupTarget(m_TargetTriple, error);
	if (!target)
	{
		nat_Throw(AotCompilerException, u8"初始化错误：无法查找目标：{0}"_nv, error);
	}

	const llvm::TargetOptions opt;
	const llvm::Optional<llvm::Reloc::Model> RM{};

	m_TargetMachine = target->createTargetMachine(m_TargetTriple, "generic", "", opt, RM, llvm::None, llvm::CodeGenOpt::Default);

	const auto dataLayout = m_TargetMachine->createDataLayout();
	m_AstContext.SetTargetInfo(TargetInfo{ dataLayout.isBigEndian() ?
		Environment::Endianness::BigEndian :
		Environment::Endianness::LittleEndian,
		dataLayout.getPointerSize(),
		dataLayout.getPointerABIAlignment(0) });

	prewarm();
}

AotCompiler::~AotCompiler()
{
}

void AotCompiler::LoadMetadata(Linq<Valued<Uri>> const& metadatas, nBool shouldCodeGen)
{
	auto& vfs = m_SourceManager.GetFileManager().GetVFS();

	Serialization::Deserializer deserializer{ m_Parser };

	for (const auto& meta : metadatas)
	{
		Metadata metadata;
		const auto request = vfs.CreateRequest(meta);
		if (!request)
		{
			nat_Throw(AotCompilerException, u8"无法创建对元数据文件 \"{0}\" 的请求", meta.GetUnderlyingString());
		}
		const auto response = request->GetResponse();
		if (!response)
		{
			nat_Throw(AotCompilerException, u8"无法获得对元数据文件 \"{0}\" 的请求的响应", meta.GetUnderlyingString());
		}
		const auto metaStream = response->GetResponseStream();
		if (!metaStream)
		{
			nat_Throw(AotCompilerException, u8"无法打开元数据文件 \"{0}\" 的流", meta.GetUnderlyingString());
		}

		auto reader = make_ref<Serialization::BinarySerializationArchiveReader>(make_ref<natBinaryReader>(metaStream, Environment::Endianness::LittleEndian));
		const auto size = deserializer.StartDeserialize(std::move(reader));
		std::vector<ASTNodePtr> ast;
		ast.reserve(size);
		for (std::size_t i = 0; i < size; ++i)
		{
			ast.emplace_back(deserializer.Deserialize());
		}
		deserializer.EndDeserialize();
		metadata.AddDecls(ast);
		m_Sema.LoadMetadata(metadata, shouldCodeGen);
	}
}

void AotCompiler::CreateMetadata(natRefPointer<natStream> const& metadataStream, nBool includeImported)
{
	assert(metadataStream && metadataStream->CanWrite() && metadataStream->CanSeek());

	auto writer = make_ref<Serialization::BinarySerializationArchiveWriter>(make_ref<natBinaryWriter>(metadataStream, Environment::Endianness::LittleEndian));
	Serialization::Serializer serializer{ m_Sema };
	serializer.StartSerialize(std::move(writer));
	//const auto metadata = m_Sema.CreateMetadata(includeImported);
	for (const auto& decl : m_AstContext.GetTranslationUnit()->GetDecls().where([includeImported, this](Declaration::DeclPtr const& declPtr)
	{
		return !declPtr->GetAttributeCount(typeid(BuiltinAttribute)) && (includeImported || !m_Sema.IsImported(declPtr));
	}).select([this](Declaration::DeclPtr const& declPtr)
	{
		m_Sema.UnmarkImported(declPtr);
		return declPtr;
	}))
	{
		serializer.Visit(decl);
	}
	serializer.EndSerialize();
}

void AotCompiler::Compile(Uri const& uri, Linq<Valued<Uri>> const& metadatas, llvm::raw_pwrite_stream& objectStream)
{
	m_Module = std::make_unique<llvm::Module>(llvm::StringRef(uri.GetPath().begin(), uri.GetPath().size()), m_LLVMContext);
	m_Module->setTargetTriple(m_TargetTriple);
	m_Module->setDataLayout(m_TargetMachine->createDataLayout());

	LoadMetadata(metadatas);

	const auto fileId = m_SourceManager.GetFileID(uri);
	const auto [succeed, content] = m_SourceManager.GetFileContent(fileId);
	if (!succeed)
	{
		nat_Throw(AotCompilerException, u8"无法取得文件 \"{0}\" 的内容"_nv, uri.GetUnderlyingString());
	}

	auto lexer = make_ref<Lex::Lexer>(fileId, content, m_Preprocessor);
	m_Preprocessor.SetLexer(std::move(lexer));
	m_Parser.ConsumeToken();
	ParseAST(m_Parser);
	if (m_DiagConsumer->IsErrored() || (EndParsingAST(m_Parser), m_DiagConsumer->IsErrored()))
	{
		m_Logger.LogErr(u8"编译文件 \"{0}\" 失败"_nv, uri.GetUnderlyingString());
		return;
	}

	std::string buffer;
	llvm::raw_string_ostream os{ buffer };
	m_Module->print(os, nullptr);
	m_Logger.LogMsg(u8"编译成功，生成的 IR:\n{0}"_nv, buffer);

	llvm::legacy::PassManager passManager;
	m_TargetMachine->addPassesToEmitFile(passManager, objectStream, nullptr, llvm::TargetMachine::CGFT_ObjectFile);
	passManager.run(*m_Module);
	objectStream.flush();

	m_Module.reset();
}

AotCompiler::AotStmtVisitor::ICleanup::~ICleanup()
{
}

AotCompiler::AotStmtVisitor::DestructorCleanup::DestructorCleanup(natRefPointer<Declaration::DestructorDecl> destructor, llvm::Value* addr)
	: m_Destructor{ std::move(destructor) }, m_Addr{ addr }
{
}

AotCompiler::AotStmtVisitor::DestructorCleanup::~DestructorCleanup()
{
}

void AotCompiler::AotStmtVisitor::DestructorCleanup::Emit(AotStmtVisitor& visitor)
{
	visitor.EmitDestructorCall(m_Destructor, m_Addr);
}

AotCompiler::AotStmtVisitor::ArrayCleanup::ArrayCleanup(natRefPointer<Type::ArrayType> type, llvm::Value* addr, CleanupFunction cleanupFunction)
	: m_Type{ std::move(type) }, m_Addr{ addr }, m_CleanupFunction{ std::move(cleanupFunction) }
{
	assert(!m_Type->GetElementType().Cast<Type::ArrayType>() && "type should be flattened.");
}

AotCompiler::AotStmtVisitor::ArrayCleanup::~ArrayCleanup()
{
}

void AotCompiler::AotStmtVisitor::ArrayCleanup::Emit(AotStmtVisitor& visitor)
{
	// 0 大小的数组不需要做任何操作
	if (!m_Type->GetSize() || !m_CleanupFunction)
	{
		return;
	}

	auto& compiler = visitor.GetCompiler();
	const auto begin = m_Addr;
	const auto end = compiler.m_IRBuilder.CreateInBoundsGEP(begin, llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(compiler.m_LLVMContext), m_Type->GetSize()));

	const auto bodyBlock = llvm::BasicBlock::Create(compiler.m_LLVMContext, "arraycleanup.body");
	const auto endBlock = llvm::BasicBlock::Create(compiler.m_LLVMContext, "arraycleanup.end");

	const auto entryBlock = compiler.m_IRBuilder.GetInsertBlock();
	visitor.EmitBlock(bodyBlock);
	const auto currentElement = llvm::PHINode::Create(begin->getType(), 2, "arraycleanup.currentElement");
	currentElement->addIncoming(end, entryBlock);

	const auto incValue = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(compiler.m_LLVMContext), -1);
	const auto element = compiler.m_IRBuilder.CreateInBoundsGEP(currentElement, incValue, "arraycleanup.element");

	m_CleanupFunction(visitor, element);

	const auto done = compiler.m_IRBuilder.CreateICmpEQ(element, begin, "arraycleanup.done");
	compiler.m_IRBuilder.CreateCondBr(done, endBlock, bodyBlock);
	currentElement->addIncoming(element, compiler.m_IRBuilder.GetInsertBlock());

	visitor.EmitBlock(endBlock);
}

AotCompiler::AotStmtVisitor::AnchorCleanup::AnchorCleanup(llvm::BasicBlock* block)
	: m_Block{ block }
{
}

AotCompiler::AotStmtVisitor::AnchorCleanup::~AnchorCleanup()
{
}

void AotCompiler::AotStmtVisitor::AnchorCleanup::Emit(AotStmtVisitor& visitor)
{
	visitor.EmitBlock(m_Block);
}

llvm::BasicBlock* AotCompiler::AotStmtVisitor::AnchorCleanup::GetBlock() const noexcept
{
	return m_Block;
}

AotCompiler::AotStmtVisitor::SpecialCleanup::SpecialCleanup(SpecialCleanupFunction cleanupFunction)
	: m_CleanupFunction{ std::move(cleanupFunction) }
{
}

AotCompiler::AotStmtVisitor::SpecialCleanup::~SpecialCleanup()
{
}

void AotCompiler::AotStmtVisitor::SpecialCleanup::Emit(AotStmtVisitor& visitor)
{
	m_CleanupFunction(visitor);
}

AotCompiler::AotStmtVisitor::LexicalScope::LexicalScope(AotStmtVisitor& visitor, SourceRange range)
	: m_AlreadyCleaned{ false }, m_AlreadyPopped{ false }, m_BeginIterator{ visitor.m_CleanupStack.begin() }, m_Visitor{ visitor }, m_Range{ range }, m_Parent{ visitor.m_CurrentLexicalScope }
{
	visitor.m_CurrentLexicalScope = this;
}

AotCompiler::AotStmtVisitor::LexicalScope::~LexicalScope()
{
	if (!m_AlreadyCleaned)
	{
		ExplicitClean();
	}

	if (!m_AlreadyPopped)
	{
		ExplicitPop();
	}
}

AotCompiler::AotStmtVisitor::LexicalScope* AotCompiler::AotStmtVisitor::LexicalScope::GetParent() const noexcept
{
	return m_Parent;
}

SourceRange AotCompiler::AotStmtVisitor::LexicalScope::GetRange() const noexcept
{
	return m_Range;
}

void AotCompiler::AotStmtVisitor::LexicalScope::AddLabel(natRefPointer<Declaration::LabelDecl> label)
{
	assert(!m_AlreadyCleaned);
	m_Labels.emplace_back(std::move(label));
}

AotCompiler::AotStmtVisitor::CleanupIterator AotCompiler::AotStmtVisitor::LexicalScope::GetBeginIterator() const noexcept
{
	return m_BeginIterator;
}

void AotCompiler::AotStmtVisitor::LexicalScope::SetBeginIterator(CleanupIterator const& iter) noexcept
{
	m_BeginIterator = iter;
	if (m_Parent)
	{
		m_Parent->EnsureEncloses(iter);
	}
}

void AotCompiler::AotStmtVisitor::LexicalScope::ExplicitClean()
{
	assert(!m_AlreadyCleaned);
	m_Visitor.PopCleanupStack(m_BeginIterator);

	m_AlreadyCleaned = true;
}

void AotCompiler::AotStmtVisitor::LexicalScope::ExplicitPop()
{
	assert(!m_AlreadyPopped);
	m_Visitor.m_CurrentLexicalScope = m_Parent;

	m_AlreadyPopped = true;
}

void AotCompiler::AotStmtVisitor::LexicalScope::SetAlreadyCleaned() noexcept
{
	m_AlreadyCleaned = true;
}

void AotCompiler::AotStmtVisitor::LexicalScope::EnsureEncloses(CleanupIterator const& iter)
{
	if (!m_Visitor.CleanupEncloses(m_BeginIterator, iter))
	{
		m_BeginIterator = iter;

		// 若本范围已经包含则不测试父范围
		if (m_Parent)
		{
			m_Parent->EnsureEncloses(iter);
		}
	}
}

void AotCompiler::prewarm()
{
	const auto topLevelNamespace = m_Sema.GetTopLevelActionNamespace();
	const auto compilerNamespace = topLevelNamespace->GetSubNamespace(u8"Compiler"_nv);
	compilerNamespace->RegisterAction(make_ref<ActionCallingConvention>());

	m_Sema.RegisterAttributeSerializer(u8"CallingConvention"_nv, make_ref<CallingConventionAttribute::CallingConventionAttributeSerializer>());
	m_Sema.RegisterAttributeSerializer(u8"Builtin"_nv, make_ref<NoDataAttributeSerializer<BuiltinAttribute>>());

	Lex::Token dummy;
	const auto nativeModule = m_Sema.ActOnModuleDecl(m_Sema.GetCurrentScope(), {}, m_Preprocessor.FindIdentifierInfo("Native", dummy));
	m_Sema.MarkAsImported(nativeModule);
	nativeModule->AttachAttribute(make_ref<BuiltinAttribute>());

	m_Sema.PushScope(Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::ModuleScope);
	m_Sema.ActOnStartModule(m_Sema.GetCurrentScope(), nativeModule);

	registerNativeType<char>(u8"Char"_nv);
	registerNativeType<wchar_t>(u8"WChar"_nv);
	registerNativeType<short>(u8"Short"_nv);
	registerNativeType<unsigned short>(u8"UShort"_nv);
	registerNativeType<int>(u8"Int"_nv);
	registerNativeType<unsigned int>(u8"UInt"_nv);
	registerNativeType<long>(u8"Long"_nv);
	registerNativeType<unsigned long>(u8"ULong"_nv);
	registerNativeType<long long>(u8"LongLong"_nv);
	registerNativeType<unsigned long long>(u8"ULongLong"_nv);

	m_Sema.ActOnAliasDeclaration(m_Sema.GetCurrentScope(), {}, m_Preprocessor.FindIdentifierInfo(u8"SizeType"_nv, dummy), {}, m_AstContext.GetSizeType());
	m_Sema.ActOnAliasDeclaration(m_Sema.GetCurrentScope(), {}, m_Preprocessor.FindIdentifierInfo(u8"PtrDiffType"_nv, dummy), {}, m_AstContext.GetPtrDiffType());

#ifdef _WIN32
	const auto win32Module = m_Sema.ActOnModuleDecl(m_Sema.GetCurrentScope(), {}, m_Preprocessor.FindIdentifierInfo("Win32", dummy));
	m_Sema.PushScope(Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::ModuleScope);
	m_Sema.ActOnStartModule(m_Sema.GetCurrentScope(), win32Module);
	registerNativeType<UINT>(u8"UINT"_nv);
	registerNativeType<DWORD>(u8"DWORD"_nv);
	m_Sema.ActOnFinishModule();
	m_Sema.PopScope();
#endif // _WIN32

	m_Sema.ActOnFinishModule();
	m_Sema.PopScope();
}

llvm::GlobalVariable* AotCompiler::getStringLiteralValue(nStrView literalContent, nStrView literalName)
{
	auto iter = m_StringLiteralPool.find(literalContent);
	if (iter != m_StringLiteralPool.end())
	{
		return iter->second;
	}

	bool succeed;
	tie(iter, succeed) = m_StringLiteralPool.emplace(literalContent, new llvm::GlobalVariable(
		*m_Module, llvm::ArrayType::get(llvm::Type::getInt8Ty(m_LLVMContext), literalContent.GetSize() + 1), true, llvm::GlobalValue::LinkageTypes::PrivateLinkage,
		llvm::ConstantDataArray::getString(m_LLVMContext, llvm::StringRef{ literalContent.begin(), literalContent.GetSize() }), llvm::StringRef{ literalName.data(), literalName.size() }));

	if (!succeed)
	{
		nat_Throw(AotCompilerException, "无法插入字符串字面量池");
	}

	// 类型的信息不会变动，缓存第一次得到的结果即可
	static const auto CharAlign = static_cast<unsigned>(m_AstContext.GetTypeInfo(m_AstContext.GetBuiltinType(Type::BuiltinType::Char)).Align);
	iter->second->setAlignment(CharAlign);

	return iter->second;
}

llvm::Type* AotCompiler::getCorrespondingType(Type::TypePtr const& type)
{
	const auto underlyingType = Type::Type::GetUnderlyingType(type);

	const auto iter = m_TypeMap.find(underlyingType);
	if (iter != m_TypeMap.cend())
	{
		return iter->second;
	}

	llvm::Type* ret;
	const auto typeClass = underlyingType->GetType();

	switch (typeClass)
	{
	case Type::Type::Builtin:
	{
		const auto builtinType = underlyingType.UnsafeCast<Type::BuiltinType>();
		switch (builtinType->GetBuiltinClass())
		{
		case Type::BuiltinType::Void:
			ret = llvm::Type::getVoidTy(m_LLVMContext);
			break;
		case Type::BuiltinType::Null:
			ret = llvm::PointerType::get(llvm::IntegerType::getInt8Ty(m_LLVMContext), 0);
			break;
		case Type::BuiltinType::Bool:
			ret = llvm::Type::getInt1Ty(m_LLVMContext);
			break;
		case Type::BuiltinType::Char:
		case Type::BuiltinType::Byte:
		case Type::BuiltinType::UShort:
		case Type::BuiltinType::UInt:
		case Type::BuiltinType::ULong:
		case Type::BuiltinType::ULongLong:
		case Type::BuiltinType::UInt128:
		case Type::BuiltinType::SByte:
		case Type::BuiltinType::Short:
		case Type::BuiltinType::Int:
		case Type::BuiltinType::Long:
		case Type::BuiltinType::LongLong:
		case Type::BuiltinType::Int128:
			ret = llvm::IntegerType::get(m_LLVMContext, static_cast<unsigned>(m_AstContext.GetTypeInfo(builtinType).Size * 8));
			break;
		case Type::BuiltinType::Float:
			ret = llvm::Type::getFloatTy(m_LLVMContext);
			break;
		case Type::BuiltinType::Double:
			ret = llvm::Type::getDoubleTy(m_LLVMContext);
			break;
		case Type::BuiltinType::LongDouble:
		case Type::BuiltinType::Float128:
			ret = llvm::Type::getFP128Ty(m_LLVMContext);
			break;
		case Type::BuiltinType::Overload:
		default:
			assert(!"Invalid BuiltinClass");
			[[fallthrough]];
		case Type::BuiltinType::Invalid:
			nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
		}
		break;
	}
	case Type::Type::Pointer:
	{
		const auto pointerType = underlyingType.UnsafeCast<Type::PointerType>();
		const auto pointeeType = pointerType->GetPointeeType();
		// TODO: 考虑地址空间的问题
		// TODO: 考虑禁用 void*
		ret = llvm::PointerType::get(getCorrespondingType(pointeeType->IsVoid() ? static_cast<Type::TypePtr>(m_AstContext.GetBuiltinType(Type::BuiltinType::Char)) : pointeeType), 0);
		break;
	}
	case Type::Type::Array:
	{
		const auto arrayType = underlyingType.UnsafeCast<Type::ArrayType>();
		ret = llvm::ArrayType::get(getCorrespondingType(arrayType->GetElementType()), static_cast<std::uint64_t>(arrayType->GetSize()));
		break;
	}
	case Type::Type::Function:
	{
		const auto functionType = underlyingType.UnsafeCast<Type::FunctionType>();
		ret = buildFunctionType(functionType->GetResultType(), functionType->GetParameterTypes(), functionType->HasVarArg());
		break;
	}
	case Type::Type::Class:
		return getCorrespondingType(underlyingType.UnsafeCast<Type::ClassType>()->GetDecl());
	case Type::Type::Enum:
		return getCorrespondingType(underlyingType.UnsafeCast<Type::EnumType>()->GetDecl().UnsafeCast<Declaration::EnumDecl>()->GetUnderlyingType());
	default:
		assert(!"Invalid type");
		[[fallthrough]];
	case Type::Type::Paren:
	case Type::Type::Auto:
		assert(!"Should never happen, check NatsuLang::Type::Type::GetUnderlyingType");
		nat_Throw(AotCompilerException, u8"错误的类型"_nv);
	}

	m_TypeMap.emplace(type, ret);
	return ret;
}

llvm::Type* AotCompiler::getCorrespondingType(Declaration::DeclPtr const& decl)
{
	const auto iter = m_DeclTypeMap.find(decl);
	if (iter != m_DeclTypeMap.cend())
	{
		return iter->second;
	}

	llvm::Type* ret;

	switch (decl->GetType())
	{
	case Declaration::Decl::Enum:
		return getCorrespondingType(decl.UnsafeCast<Declaration::EnumDecl>()->GetTypeForDecl());
	case Declaration::Decl::Class:
		return buildClassType(decl.UnsafeCast<Declaration::ClassDecl>());
	case Declaration::Decl::Field:
		return getCorrespondingType(decl.UnsafeCast<Declaration::FieldDecl>()->GetValueType());
	case Declaration::Decl::Function:
		ret = buildFunctionType(decl.UnsafeCast<Declaration::FunctionDecl>());
		break;
	case Declaration::Decl::Method:
	case Declaration::Decl::Constructor:
	case Declaration::Decl::Destructor:
		ret = buildFunctionType(decl.UnsafeCast<Declaration::MethodDecl>());
		break;
	case Declaration::Decl::Var:
		return getCorrespondingType(decl.UnsafeCast<Declaration::VarDecl>()->GetValueType());
	case Declaration::Decl::EnumConstant:
		return getCorrespondingType(decl.UnsafeCast<Declaration::EnumConstantDecl>()->GetValueType());
	case Declaration::Decl::ParmVar:
		return getCorrespondingType(decl.UnsafeCast<Declaration::ParmVarDecl>()->GetValueType());
	case Declaration::Decl::Empty:
	case Declaration::Decl::Import:
	case Declaration::Decl::Label:
	case Declaration::Decl::Module:
	case Declaration::Decl::Unresolved:
	case Declaration::Decl::ImplicitParam:
	case Declaration::Decl::TranslationUnit:
	default:
		assert(!"Invalid decl.");
		nat_Throw(AotCompilerException, u8"不能为此声明确定类型"_nv);
	}

	m_DeclTypeMap.emplace(decl, ret);
	return ret;
}

llvm::Type* AotCompiler::buildFunctionType(Type::TypePtr const& resultType, Linq<Valued<Type::TypePtr>> const& params, nBool hasVarArg)
{
	const auto args{ params.select([this](Type::TypePtr const& paramType)
	{
		return getCorrespondingType(paramType);
	}).Cast<std::vector<llvm::Type*>>() };

	return llvm::FunctionType::get(getCorrespondingType(resultType), args, hasVarArg);
}

llvm::Type* AotCompiler::buildFunctionTypeWithParamDecl(Type::TypePtr const& resultType, Linq<Valued<natRefPointer<Declaration::ParmVarDecl>>> const& params, nBool hasVarArg)
{
	const auto args{ params.select([this](natRefPointer<Declaration::ParmVarDecl> const& paramDecl)
	{
		return getCorrespondingType(paramDecl);
	}).Cast<std::vector<llvm::Type*>>() };

	return llvm::FunctionType::get(getCorrespondingType(resultType), args, hasVarArg);
}

llvm::Type* AotCompiler::buildFunctionType(natRefPointer<Declaration::FunctionDecl> const& funcDecl)
{
	const auto functionType = funcDecl->GetValueType().UnsafeCast<Type::FunctionType>();
	return buildFunctionType(functionType->GetResultType(), functionType->GetParameterTypes(), functionType->HasVarArg());
}

llvm::Type* AotCompiler::buildFunctionType(natRefPointer<Declaration::MethodDecl> const& methodDecl)
{
	const auto functionType = methodDecl->GetValueType().UnsafeCast<Type::FunctionType>();
	const auto classDecl = dynamic_cast<Declaration::ClassDecl*>(Declaration::Decl::CastFromDeclContext(methodDecl->GetContext()));
	assert(classDecl);
	return buildFunctionType(functionType->GetResultType(), from_values({ m_AstContext.GetPointerType(classDecl->GetTypeForDecl()).UnsafeCast<Type::Type>() })
		.concat(functionType->GetParameterTypes()), functionType->HasVarArg());
}

llvm::Type* AotCompiler::buildClassType(natRefPointer<Declaration::ClassDecl> const& classDecl)
{
	const auto className = classDecl->GetName();
	const auto structType = llvm::StructType::create(m_LLVMContext, llvm::StringRef{ className.data(), className.size() });
	m_DeclTypeMap.emplace(classDecl, structType);

	const auto& classLayout = m_AstContext.GetClassLayout(classDecl);
	std::vector<llvm::Type*> fieldTypes(classLayout.FieldOffsets.size());

	const auto paddingElementType = llvm::Type::getInt8Ty(m_LLVMContext);

	for (std::size_t i = 0; i < classLayout.FieldOffsets.size(); ++i)
	{
		const auto& pair = classLayout.FieldOffsets[i];
		if (pair.first)
		{
			fieldTypes[i] = getCorrespondingType(pair.first->GetValueType());
		}
		else
		{
			fieldTypes[i] = llvm::ArrayType::get(paddingElementType,
				static_cast<std::uint64_t>((i != classLayout.FieldOffsets.size() - 1 ? classLayout.FieldOffsets[i + 1].second : classLayout.Size) - pair.second));
		}
	}

	structType->setBody(fieldTypes);
	return structType;
}

natRefPointer<Type::ArrayType> AotCompiler::flattenArray(natRefPointer<Type::ArrayType> arrayType)
{
	// 也可能是本来元素类型就是 nullptr，这里不考虑这个情况，但是这是可能的错误点
	while (const auto elemArrayType = arrayType->GetElementType().Cast<Type::ArrayType>())
	{
		arrayType = m_AstContext.GetArrayType(elemArrayType->GetElementType(), arrayType->GetSize() * elemArrayType->GetSize());
	}

	return arrayType;
}

natRefPointer<Declaration::DestructorDecl> AotCompiler::findDestructor(natRefPointer<Declaration::ClassDecl> const& classDecl)
{
	assert(classDecl);
	return classDecl->GetDecls().select([](Declaration::DeclPtr const& decl)
	{
		return decl.Cast<Declaration::DestructorDecl>();
	}).where([](natRefPointer<Declaration::DestructorDecl> const& method) -> nBool
	{
		return method;
	}).first_or_default(nullptr);
}
