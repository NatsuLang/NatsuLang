#include "CodeGen.h"

#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>

using namespace NatsuLib;
using namespace NatsuLang;
using namespace Compiler;

AotCompiler::AotDiagIdMap::AotDiagIdMap(NatsuLib::natRefPointer<NatsuLib::TextReader<NatsuLib::StringType::Utf8>> const& reader)
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

	const auto loc = diag.GetSourceLocation();
	if (loc.GetFileID())
	{
		const auto [succeed, fileContent] = m_Compiler.m_SourceManager.GetFileContent(loc.GetFileID());
		if (const auto line = loc.GetLineInfo(); succeed && line)
		{
			size_t offset{};
			for (nuInt i = 1; i < line; ++i)
			{
				offset = fileContent.Find(Environment::GetNewLine(), static_cast<ptrdiff_t>(offset));
				if (offset == nStrView::npos)
				{
					// TODO: 无法定位到源文件
					return;
				}

				offset += Environment::GetNewLine().GetSize();
			}

			const auto nextNewLine = fileContent.Find(Environment::GetNewLine(), static_cast<ptrdiff_t>(offset));
			const auto column = loc.GetColumnInfo();
			offset += column ? column - 1 : 0;
			if (nextNewLine <= offset)
			{
				// TODO: 无法定位到源文件
				return;
			}

			m_Compiler.m_Logger.Log(levelId, fileContent.Slice(static_cast<ptrdiff_t>(offset), nextNewLine == nStrView::npos ? -1 : static_cast<ptrdiff_t>(nextNewLine)));
			m_Compiler.m_Logger.Log(levelId, u8"^"_nv);
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
			const auto functionName = funcDecl->GetIdentifierInfo()->GetName();

			const auto funcValue = llvm::Function::Create(functionType,
				llvm::GlobalVariable::ExternalLinkage,
				std::string(functionName.cbegin(), functionName.cend()),
				m_Compiler.m_Module.get());

			auto argIter = funcValue->arg_begin();
			const auto argEnd = funcValue->arg_end();
			auto paramIter = funcDecl->GetParams().begin();
			const auto paramEnd = funcDecl->GetParams().end();

			if (funcDecl.Cast<Declaration::MethodDecl>())
			{
				argIter->setName("this");
				++argIter;
			}

			for (; argIter != argEnd && paramIter != paramEnd; ++argIter, static_cast<void>(++paramIter))
			{
				const auto name = (*paramIter)->GetIdentifierInfo()->GetName();
				const std::string nameStr{ name.cbegin(), name.cend() };
				argIter->setName(nameStr);
			}

			m_Compiler.m_FunctionMap.emplace(std::move(funcDecl), funcValue);
			decl.second = funcValue;
		}
		else if (auto varDecl = decl.first.Cast<Declaration::VarDecl>())
		{
			const auto varType = m_Compiler.getCorrespondingType(varDecl->GetValueType());
			const auto varName = varDecl->GetName();

			const auto initializer = varDecl->GetInitializer();

			llvm::Constant* initValue{};

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

			const auto varValue = new llvm::GlobalVariable(*m_Compiler.m_Module, varType, false, llvm::GlobalValue::ExternalLinkage, initValue, llvm::StringRef{ varName.data(), varName.size() });
			m_Compiler.m_GlobalVariableMap.emplace(std::move(varDecl), varValue);
			decl.second = varValue;
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
				break;
			}
			case Specifier::StorageClass::Extern:
				break;
			default:
				break;
			}
		}
		else if (const auto varDecl = decl.first.Cast<Declaration::VarDecl>())
		{
			// TODO: 动态初始化
		}
	}

	return true;
}

AotCompiler::AotStmtVisitor::AotStmtVisitor(AotCompiler& compiler, natRefPointer<Declaration::FunctionDecl> funcDecl, llvm::Function* funcValue)
	: m_Compiler{ compiler }, m_CurrentFunction{ std::move(funcDecl) }, m_CurrentFunctionValue{ funcValue },
	  m_This{}, m_LastVisitedValue{ nullptr }, m_RequiredModifiableValue{ false }
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
		llvm::IRBuilder<> entryIRBuilder{ &m_CurrentFunctionValue->getEntryBlock(), m_CurrentFunctionValue->getEntryBlock().begin() };
		const auto arg = entryIRBuilder.CreateAlloca(argIter->getType(), nullptr, argIter->getName());
		m_Compiler.m_IRBuilder.CreateStore(&*argIter, arg);

		m_DeclMap.emplace(*paramIter, arg);
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
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
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
	for (auto&& item : stmt->GetChildrens())
	{
		Visit(item);
	}
}

void AotCompiler::AotStmtVisitor::VisitContinueStmt(natRefPointer<Statement::ContinueStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitDeclStmt(natRefPointer<Statement::DeclStmt> const& stmt)
{
	for (auto const& decl : stmt->GetDecls())
	{
		if (!decl)
		{
			nat_Throw(AotCompilerException, u8"错误的声明"_nv);
		}

		if (auto varDecl = decl.Cast<Declaration::VarDecl>())
		{
			if (varDecl->IsFunction())
			{
				// 目前只能在顶层声明函数
				continue;
			}

			const auto type = varDecl->GetValueType();

			// 可能用于动态大小的类型
			llvm::Value* arraySize = nullptr;

			const auto varName = varDecl->GetName();
			const auto valueType = m_Compiler.getCorrespondingType(type);
			const auto typeInfo = m_Compiler.m_AstContext.GetTypeInfo(type);

			if (varDecl->GetStorageClass() == Specifier::StorageClass::None)
			{
				const auto storage = m_Compiler.m_IRBuilder.CreateAlloca(valueType, arraySize, std::string(varName.cbegin(), varName.cend()));
				storage->setAlignment(static_cast<unsigned>(typeInfo.Align));

				InitVar(type, storage, varDecl->GetInitializer());

				m_DeclMap.emplace(varDecl, storage);
			}
		}
	}
}

void AotCompiler::AotStmtVisitor::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	const auto loopEnd = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "do.end");
	const auto loopCond = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "do.cond");

	const auto loopBody = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "do.body");
	EmitBlock(loopBody);
	Visit(stmt->GetBody());

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

		m_LastVisitedValue = phi;
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
		m_LastVisitedValue = m_Compiler.m_IRBuilder.CreateLoad(m_LastVisitedValue, "arrayElem");
	}
}

void AotCompiler::AotStmtVisitor::VisitBinaryOperator(natRefPointer<Expression::BinaryOperator> const& expr)
{
	const auto builtinLeftOperandType = expr->GetLeftOperand()->GetExprType().Cast<Type::BuiltinType>();
	EvaluateValue(expr->GetLeftOperand());
	const auto leftOperand = m_LastVisitedValue;

	const auto builtinRightOperandType = expr->GetRightOperand()->GetExprType().Cast<Type::BuiltinType>();
	EvaluateValue(expr->GetRightOperand());
	const auto rightOperand = m_LastVisitedValue;

	const auto opCode = expr->GetOpcode();
	// 左右操作数类型应当相同
	const auto commonType = expr->GetLeftOperand()->GetExprType();
	const auto resultType = expr->GetExprType().Cast<Type::BuiltinType>();

	m_LastVisitedValue = EmitBinOp(leftOperand, rightOperand, opCode, commonType, resultType);
}

void AotCompiler::AotStmtVisitor::VisitCompoundAssignOperator(natRefPointer<Expression::CompoundAssignOperator> const& expr)
{
	const auto resultType = expr->GetLeftOperand()->GetExprType();

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
	case Expression::BinaryOperationType::MulAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Mul, resultType, resultType);
		break;
	case Expression::BinaryOperationType::DivAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Div, resultType, resultType);
		break;
	case Expression::BinaryOperationType::RemAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Mod, resultType, resultType);
		break;
	case Expression::BinaryOperationType::AddAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Add, resultType, resultType);
		break;
	case Expression::BinaryOperationType::SubAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Sub, resultType, resultType);
		break;
	case Expression::BinaryOperationType::ShlAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Shl, resultType, resultType);
		break;
	case Expression::BinaryOperationType::ShrAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Shr, resultType, resultType);
		break;
	case Expression::BinaryOperationType::AndAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::And, resultType, resultType);
		break;
	case Expression::BinaryOperationType::XorAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Xor, resultType, resultType);
		break;
	case Expression::BinaryOperationType::OrAssign:
		value = EmitBinOp(m_Compiler.m_IRBuilder.CreateLoad(leftOperand), rightOperand, Expression::BinaryOperationType::Or, resultType, resultType);
		break;
	default:
		assert(!"Invalid Opcode");
		nat_Throw(AotCompilerException, u8"无效的 Opcode"_nv);
	}

	m_Compiler.m_IRBuilder.CreateStore(value, leftOperand);

	m_LastVisitedValue = m_RequiredModifiableValue ? leftOperand : m_Compiler.m_IRBuilder.CreateLoad(leftOperand);
}

void AotCompiler::AotStmtVisitor::VisitBooleanLiteral(natRefPointer<Expression::BooleanLiteral> const& expr)
{
	m_LastVisitedValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(m_Compiler.m_LLVMContext), expr->GetValue());
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
	m_LastVisitedValue = m_RequiredModifiableValue ? m_This : m_Compiler.m_IRBuilder.CreateLoad(m_This);
}

void AotCompiler::AotStmtVisitor::VisitThrowExpr(natRefPointer<Expression::ThrowExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCallExpr(natRefPointer<Expression::CallExpr> const& expr)
{
	EvaluateValue(expr->GetCallee());
	const auto callee = llvm::cast<llvm::Function>(m_LastVisitedValue);
	assert(callee);

	if (callee->arg_size() != expr->GetArgCount())
	{
		nat_Throw(AotCompilerException, u8"参数数量不匹配，这可能是默认参数功能未实现导致的"_nv);
	}

	std::vector<llvm::Value*> args;
	args.reserve(expr->GetArgCount());

	// TODO: 实现默认参数
	for (auto&& arg : expr->GetArgs())
	{
		// TODO: 直接按位复制了，在需要的时候应由前端生成复制构造函数，但此处没有看到分配存储？
		EvaluateValue(arg);
		assert(m_LastVisitedValue);
		args.emplace_back(m_LastVisitedValue);
	}

	const auto callInst = m_Compiler.m_IRBuilder.CreateCall(callee, args);
	if (!callee->getReturnType()->isVoidTy())
	{
		callInst->setName("ret");
	}

	m_LastVisitedValue = callInst;
}

void AotCompiler::AotStmtVisitor::VisitMemberCallExpr(natRefPointer<Expression::MemberCallExpr> const& expr)
{
	EvaluateValue(expr->GetCallee());
	const auto callee = llvm::cast<llvm::Function>(m_LastVisitedValue);
	assert(callee);

	const auto baseObj = expr->GetImplicitObjectArgument();
	EvaluateAsModifiableValue(baseObj);
	const auto baseObjValue = m_LastVisitedValue;

	assert(callee && baseObjValue);

	// TODO: 消除重复代码

	// 除去 this
	if (callee->arg_size() - 1 != expr->GetArgCount())
	{
		nat_Throw(AotCompilerException, u8"参数数量不匹配，这可能是默认参数功能未实现导致的"_nv);
	}

	// 将基础对象的引用作为第一个参数传入
	std::vector<llvm::Value*> args{ baseObjValue };
	args.reserve(expr->GetArgCount() + 1);

	// TODO: 实现默认参数
	for (auto&& arg : expr->GetArgs())
	{
		// TODO: 直接按位复制了，在需要的时候应由前端生成复制构造函数，但此处没有看到分配存储？
		EvaluateValue(arg);
		assert(m_LastVisitedValue);
		args.emplace_back(m_LastVisitedValue);
	}

	const auto callInst = m_Compiler.m_IRBuilder.CreateCall(callee, args);
	if (!callee->getReturnType()->isVoidTy())
	{
		callInst->setName("ret");
	}

	m_LastVisitedValue = callInst;
}

void AotCompiler::AotStmtVisitor::VisitCastExpr(natRefPointer<Expression::CastExpr> const& expr)
{
	const auto operand = expr->GetOperand();
	EvaluateValue(operand);
	m_LastVisitedValue = ConvertScalarTo(m_LastVisitedValue, operand->GetExprType(), expr->GetExprType());
}

void AotCompiler::AotStmtVisitor::VisitAsTypeExpr(natRefPointer<Expression::AsTypeExpr> const& expr)
{
	const auto operand = expr->GetOperand();
	EvaluateValue(operand);
	m_LastVisitedValue = ConvertScalarTo(m_LastVisitedValue, operand->GetExprType(), expr->GetExprType());
}

void AotCompiler::AotStmtVisitor::VisitImplicitCastExpr(natRefPointer<Expression::ImplicitCastExpr> const& expr)
{
	const auto operand = expr->GetOperand();
	EvaluateValue(operand);
	m_LastVisitedValue = ConvertScalarTo(m_LastVisitedValue, operand->GetExprType(), expr->GetExprType());
}

void AotCompiler::AotStmtVisitor::VisitCharacterLiteral(natRefPointer<Expression::CharacterLiteral> const& expr)
{
	m_LastVisitedValue = llvm::ConstantInt::get(m_Compiler.getCorrespondingType(expr->GetExprType()), expr->GetCodePoint());
}

void AotCompiler::AotStmtVisitor::VisitDeclRefExpr(natRefPointer<Expression::DeclRefExpr> const& expr)
{
	const auto decl = expr->GetDecl();

	if (const auto varDecl = decl.Cast<Declaration::VarDecl>())
	{
		if (const auto funcDecl = varDecl.Cast<Declaration::FunctionDecl>())
		{
			if (!m_RequiredModifiableValue)
			{
				const auto funcIter = m_Compiler.m_FunctionMap.find(decl);
				if (funcIter != m_Compiler.m_FunctionMap.end())
				{
					m_LastVisitedValue = funcIter->second;
					return;
				}
			}
			else
			{
				nat_Throw(AotCompilerException, u8"无法修改函数地址"_nv);
			}
		}
		else
		{
			EmitAddressOfVar(varDecl);
			if (!m_RequiredModifiableValue)
			{
				m_LastVisitedValue = m_Compiler.m_IRBuilder.CreateLoad(m_LastVisitedValue, "var");
			}
			return;
		}
	}

	nat_Throw(AotCompilerException, u8"定义引用表达式引用了不存在或不合法的定义"_nv);
}

void AotCompiler::AotStmtVisitor::VisitFloatingLiteral(natRefPointer<Expression::FloatingLiteral> const& expr)
{
	const auto floatType = expr->GetExprType().Cast<Type::BuiltinType>();

	if (floatType->GetBuiltinClass() == Type::BuiltinType::Float)
	{
		m_LastVisitedValue = llvm::ConstantFP::get(m_Compiler.m_LLVMContext, llvm::APFloat{ static_cast<nFloat>(expr->GetValue()) });
	}
	else
	{
		m_LastVisitedValue = llvm::ConstantFP::get(m_Compiler.m_LLVMContext, llvm::APFloat{ expr->GetValue() });
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

	m_LastVisitedValue = llvm::ConstantInt::get(m_Compiler.m_LLVMContext, llvm::APInt{ static_cast<unsigned>(typeInfo.Size * 8), expr->GetValue(), intType->IsSigned() });
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

		m_LastVisitedValue = iter->second;
	}
	else if (const auto field = memberDecl.Cast<Declaration::FieldDecl>())
	{
		const auto baseExpr = expr->GetBase();
		EvaluateAsModifiableValue(baseExpr);
		const auto baseValue = m_LastVisitedValue;
		const auto baseClass = baseExpr->GetExprType().Cast<Type::ClassType>();
		assert(baseClass);
		const auto baseClassDecl = baseClass->GetDecl();

		const auto& classLayout = m_Compiler.m_AstContext.GetClassLayout(baseClassDecl);
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
		m_LastVisitedValue = m_RequiredModifiableValue ? memberPtr : m_Compiler.m_IRBuilder.CreateLoad(memberPtr, llvm::StringRef{ fieldName.data(), fieldName.size() });
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
	m_LastVisitedValue = m_Compiler.getStringLiteralValue(expr->GetValue());
}

void AotCompiler::AotStmtVisitor::VisitUnaryExprOrTypeTraitExpr(natRefPointer<Expression::UnaryExprOrTypeTraitExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
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
		m_LastVisitedValue = EmitIncDec(m_LastVisitedValue, opType, true, false);
		break;
	case Expression::UnaryOperationType::PreInc:
		EvaluateAsModifiableValue(operand);
		m_LastVisitedValue = EmitIncDec(m_LastVisitedValue, opType, true, true);
		break;
	case Expression::UnaryOperationType::PostDec:
		EvaluateAsModifiableValue(operand);
		m_LastVisitedValue = EmitIncDec(m_LastVisitedValue, opType, false, false);
		break;
	case Expression::UnaryOperationType::PreDec:
		EvaluateAsModifiableValue(operand);
		m_LastVisitedValue = EmitIncDec(m_LastVisitedValue, opType, false, true);
		break;
	case Expression::UnaryOperationType::AddrOf:
		// 返回值即为地址
		EvaluateAsModifiableValue(operand);
		break;
	case Expression::UnaryOperationType::Deref:
		EvaluateValue(operand);
		if (!m_RequiredModifiableValue)
		{
			m_LastVisitedValue = m_Compiler.m_IRBuilder.CreateLoad(m_LastVisitedValue, "deref");
		}

		break;
	case Expression::UnaryOperationType::Plus:
		EvaluateValue(operand);
		break;
	case Expression::UnaryOperationType::Minus:
		EvaluateValue(operand);
		m_LastVisitedValue = m_Compiler.m_IRBuilder.CreateNeg(m_LastVisitedValue);
		break;
	case Expression::UnaryOperationType::Not:
		EvaluateValue(operand);
		m_LastVisitedValue = m_Compiler.m_IRBuilder.CreateNot(m_LastVisitedValue);
		break;
	case Expression::UnaryOperationType::LNot:
		EvaluateAsBool(operand);
		m_LastVisitedValue = m_Compiler.m_IRBuilder.CreateIsNull(m_LastVisitedValue);
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
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitGotoStmt(natRefPointer<Statement::GotoStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitIfStmt(natRefPointer<Statement::IfStmt> const& stmt)
{
	Visit(stmt->GetCond());
	const auto condExpr = m_LastVisitedValue;

	const auto thenStmt = stmt->GetThen();
	const auto elseStmt = stmt->GetElse();

	const auto trueBranch = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "if.then");
	const auto endBranch = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "if.end");
	const auto falseBranch = elseStmt ? llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "if.else") : endBranch;

	m_Compiler.m_IRBuilder.CreateCondBr(condExpr, trueBranch, falseBranch);

	EmitBlock(trueBranch);
	Visit(thenStmt);
	EmitBranch(endBranch);

	if (elseStmt)
	{
		EmitBlock(falseBranch);
		Visit(elseStmt);
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

void AotCompiler::AotStmtVisitor::VisitReturnStmt(natRefPointer<Statement::ReturnStmt> const& stmt)
{
	if (const auto retExpr = stmt->GetReturnExpr())
	{
		Visit(retExpr);
		m_Compiler.m_IRBuilder.CreateRet(m_LastVisitedValue);
	}
	else
	{
		m_Compiler.m_IRBuilder.CreateRetVoid();
	}
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
	Visit(stmt->GetBody());

	EmitBranch(loopHead);

	EmitBlock(loopEnd);
}

void AotCompiler::AotStmtVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::StartVisit()
{
	Visit(m_CurrentFunction->GetBody());

	// TODO: 改为前端实现
	if (m_CurrentFunction->GetValueType().Cast<Type::FunctionType>()->GetResultType()->IsVoid() && !m_Compiler.m_IRBuilder.GetInsertBlock()->getTerminator())
	{
		m_Compiler.m_IRBuilder.CreateRetVoid();
	}
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

void AotCompiler::AotStmtVisitor::EmitAddressOfVar(NatsuLib::natRefPointer<Declaration::VarDecl> const& varDecl)
{
	assert(varDecl);

	const auto localDeclIter = m_DeclMap.find(varDecl);
	if (localDeclIter != m_DeclMap.end())
	{
		m_LastVisitedValue = localDeclIter->second;
		return;
	}

	const auto nonLocalDeclIter = m_Compiler.m_GlobalVariableMap.find(varDecl);
	if (nonLocalDeclIter != m_Compiler.m_GlobalVariableMap.end())
	{
		m_LastVisitedValue = nonLocalDeclIter->second;
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

llvm::Value* AotCompiler::AotStmtVisitor::EmitBinOp(llvm::Value* leftOperand, llvm::Value* rightOperand, Expression::BinaryOperationType opCode, natRefPointer<Type::BuiltinType> const& commonType, natRefPointer<Type::BuiltinType> const& resultType)
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
	case Expression::BinaryOperationType::Mod:
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

llvm::Value* AotCompiler::AotStmtVisitor::EmitIncDec(llvm::Value* operand, natRefPointer<Type::BuiltinType> const& opType, nBool isInc, nBool isPre)
{
	llvm::Value* value = m_Compiler.m_IRBuilder.CreateLoad(operand);

	if (opType->IsFloatingType())
	{
		value = m_Compiler.m_IRBuilder.CreateFAdd(value, llvm::ConstantFP::get(m_Compiler.getCorrespondingType(opType), isInc ? 1.0 : -1.0));
	}
	else
	{
		value = m_Compiler.m_IRBuilder.CreateAdd(value, llvm::ConstantInt::get(m_Compiler.getCorrespondingType(opType), isInc ? 1 : -1, opType->IsSigned()));
	}

	m_Compiler.m_IRBuilder.CreateStore(value, operand);
	return isPre ? operand : value;
}

void AotCompiler::AotStmtVisitor::InitVar(Type::TypePtr const& varType, llvm::Value* varPtr, Expression::ExprPtr const& initializer)
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

					InitVar(arrayType->GetElementType(), elemPtr, initExpr);
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
					EvaluateValue(initListExpr->GetInitExprs().first());
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
			const auto iter = m_Compiler.m_FunctionMap.find(constructExpr->GetConstructorDecl());
			if (iter == m_Compiler.m_FunctionMap.end())
			{
				nat_Throw(AotCompilerException, u8"构造表达式引用了不存在的构造函数"_nv);
			}

			const auto constructorValue = iter->second;

			// TODO: 消除重复代码

			// 除去 this
			if (constructorValue->arg_size() - 1 != constructExpr->GetArgCount())
			{
				nat_Throw(AotCompilerException, u8"参数数量不匹配，这可能是默认参数功能未实现导致的"_nv);
			}

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
			m_Compiler.m_IRBuilder.CreateMemCpy(varPtr, stringLiteralPtr, literalValue.GetSize() + 1, static_cast<unsigned>(typeInfo.Align));
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

void AotCompiler::AotStmtVisitor::EvaluateValue(Expression::ExprPtr const& expr)
{
	assert(expr);

	const auto oldValue = m_RequiredModifiableValue;
	m_RequiredModifiableValue = false;
	const auto scope = make_scope([this, oldValue]
	{
		m_RequiredModifiableValue = oldValue;
	});

	Visit(expr);
}

void AotCompiler::AotStmtVisitor::EvaluateAsModifiableValue(Expression::ExprPtr const& expr)
{
	assert(expr);

	const auto oldValue = m_RequiredModifiableValue;
	m_RequiredModifiableValue = true;
	const auto scope = make_scope([this, oldValue]
	{
		m_RequiredModifiableValue = oldValue;
	});

	Visit(expr);
}

void AotCompiler::AotStmtVisitor::EvaluateAsBool(Expression::ExprPtr const& expr)
{
	assert(expr);

	EvaluateValue(expr);

	m_LastVisitedValue = ConvertScalarToBool(m_LastVisitedValue, expr->GetExprType());
}

llvm::Value* AotCompiler::AotStmtVisitor::ConvertScalarTo(llvm::Value* from, Type::TypePtr fromType, Type::TypePtr toType)
{
	fromType = Type::Type::GetUnderlyingType(fromType);
	toType = Type::Type::GetUnderlyingType(toType);

	if (fromType == toType)
	{
		return from;
	}

	const auto builtinFromType = fromType.Cast<Type::BuiltinType>(), builtinToType = toType.Cast<Type::BuiltinType>();

	if (!builtinFromType || !builtinToType)
	{
		// TODO: 报告错误
		return nullptr;
	}

	if (builtinToType->GetBuiltinClass() == Type::BuiltinType::Void)
	{
		return nullptr;
	}

	if (builtinToType->GetBuiltinClass() == Type::BuiltinType::Bool)
	{
		return ConvertScalarToBool(from, builtinFromType);
	}

	const auto llvmFromType = m_Compiler.getCorrespondingType(fromType), llvmToType = m_Compiler.getCorrespondingType(toType);

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

llvm::Value* AotCompiler::AotStmtVisitor::ConvertScalarToBool(llvm::Value* from, natRefPointer<Type::BuiltinType> const& fromType)
{
	assert(from && fromType);

	if (fromType->GetBuiltinClass() == Type::BuiltinType::Bool)
	{
		return from;
	}

	if (fromType->IsFloatingType())
	{
		const auto floatingZero = llvm::ConstantFP::getNullValue(from->getType());
		return m_Compiler.m_IRBuilder.CreateFCmpUNE(from, floatingZero, "floatingtobool");
	}

	assert(fromType->IsIntegerType());
	return m_Compiler.m_IRBuilder.CreateIsNotNull(from, "inttobool");
}

AotCompiler::AotCompiler(natRefPointer<TextReader<StringType::Utf8>> const& diagIdMapFile, natLog& logger)
	: m_DiagConsumer{ make_ref<AotDiagConsumer>(*this) },
	m_Diag{ make_ref<AotDiagIdMap>(diagIdMapFile), m_DiagConsumer },
	m_Logger{ logger },
	m_SourceManager{ m_Diag, m_FileManager },
	m_Preprocessor{ m_Diag, m_SourceManager },
	m_Consumer{ make_ref<AotAstConsumer>(*this) },
	m_Sema{ m_Preprocessor, m_AstContext, m_Consumer },
	m_Parser{ m_Preprocessor, m_Sema },
	m_IRBuilder{ m_LLVMContext }
{
	LLVMInitializeX86TargetInfo();
	LLVMInitializeX86Target();
	LLVMInitializeX86TargetMC();
	LLVMInitializeX86AsmParser();
	LLVMInitializeX86AsmPrinter();
}

AotCompiler::~AotCompiler()
{
}

void AotCompiler::Compile(Uri const& uri, llvm::raw_pwrite_stream& stream)
{
	const auto path = uri.GetPath();
	m_Module = std::make_unique<llvm::Module>(std::string(path.begin(), path.end()), m_LLVMContext);
	const auto targetTriple = llvm::sys::getDefaultTargetTriple();
	m_Module->setTargetTriple(targetTriple);
	std::string error;
	const auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
	if (!target)
	{
		m_Logger.LogErr(u8"{0}"_nv, error);
		return;
	}

	const llvm::TargetOptions opt;
	const llvm::Optional<llvm::Reloc::Model> RM{};
	const auto machine = target->createTargetMachine(targetTriple, "generic", "", opt, RM, llvm::None, llvm::CodeGenOpt::Default);
	m_Module->setDataLayout(machine->createDataLayout());

	const auto fileId = m_SourceManager.GetFileID(uri);
	const auto [succeed, content] = m_SourceManager.GetFileContent(fileId);
	if (!succeed)
	{
		nat_Throw(AotCompilerException, u8"无法取得文件 \"{0}\" 的内容"_nv, uri.GetUnderlyingString());
	}

	auto lexer = make_ref<Lex::Lexer>(content, m_Preprocessor);
	lexer->SetFileID(fileId);
	m_Preprocessor.SetLexer(std::move(lexer));
	m_Parser.ConsumeToken();
	ParseAST(m_Parser);
	EndParsingAST(m_Parser);

	if (m_DiagConsumer->IsErrored())
	{
		m_Logger.LogErr(u8"编译文件 \"{0}\" 失败"_nv, uri.GetUnderlyingString());
		return;
	}

#if !defined(NDEBUG)
	std::string buffer;
	llvm::raw_string_ostream os{ buffer };
	m_Module->print(os, nullptr);
	m_Logger.LogMsg(u8"编译成功，生成的 IR:\n{0}"_nv, buffer);
#endif // NDEBUG

	llvm::legacy::PassManager passManager;
	machine->addPassesToEmitFile(passManager, stream, llvm::TargetMachine::CGFT_ObjectFile);
	passManager.run(*m_Module);
	stream.flush();

	m_Module.reset();
}

llvm::GlobalVariable* AotCompiler::getStringLiteralValue(nStrView literalContent, nStrView literalName)
{
	auto iter = m_StringLiteralPool.find(literalContent);
	if (iter != m_StringLiteralPool.end())
	{
		return iter->second.get();
	}

	bool succeed;
	tie(iter, succeed) = m_StringLiteralPool.emplace(literalContent, std::make_unique<llvm::GlobalVariable>(
		llvm::ArrayType::get(llvm::Type::getInt8Ty(m_LLVMContext), literalContent.GetSize() + 1), true, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
		llvm::ConstantDataArray::getString(m_LLVMContext, llvm::StringRef{ literalContent.begin(), literalContent.GetSize() }), llvm::StringRef{ literalName.data(), literalName.size() }));

	if (!succeed)
	{
		nat_Throw(AotCompilerException, "无法插入字符串字面量池");
	}

	// 类型的信息不会变动，缓存第一次得到的结果即可
	static const auto CharAlign = static_cast<unsigned>(m_AstContext.GetTypeInfo(m_AstContext.GetBuiltinType(Type::BuiltinType::Char)).Align);
	iter->second->setAlignment(CharAlign);

	return iter->second.get();
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
		case Type::BuiltinType::Bool:
			ret = llvm::Type::getInt1Ty(m_LLVMContext);
			break;
		case Type::BuiltinType::Char:
		case Type::BuiltinType::UShort:
		case Type::BuiltinType::UInt:
		case Type::BuiltinType::ULong:
		case Type::BuiltinType::ULongLong:
		case Type::BuiltinType::UInt128:
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
		case Type::BuiltinType::BoundMember:
		case Type::BuiltinType::BuiltinFn:
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
		// TODO: 考虑地址空间的问题
		ret = llvm::PointerType::get(getCorrespondingType(pointerType->GetPointeeType()), 0);
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
		ret = buildFunctionType(functionType->GetResultType(), functionType->GetParameterTypes());
		break;
	}
	case Type::Type::Class:
		return getCorrespondingType(underlyingType.UnsafeCast<Type::ClassType>()->GetDecl());
	case Type::Type::Enum:
		nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
	default:
		assert(!"Invalid type");
		[[fallthrough]];
	case Type::Type::Paren:
	case Type::Type::TypeOf:
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
		ret = buildClassType(decl.UnsafeCast<Declaration::ClassDecl>());
		break;
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
	case Declaration::Decl::Empty:
	case Declaration::Decl::Import:
	case Declaration::Decl::Label:
	case Declaration::Decl::Module:
	case Declaration::Decl::Unresolved:
	case Declaration::Decl::ImplicitParam:
	case Declaration::Decl::ParmVar:
	case Declaration::Decl::TranslationUnit:
	default:
		assert(!"Invalid decl.");
		nat_Throw(AotCompilerException, u8"不能为此声明确定类型"_nv);
	}

	m_DeclTypeMap.emplace(decl, ret);
	return ret;
}

llvm::Type* AotCompiler::buildFunctionType(Type::TypePtr const& resultType, Linq<Valued<Type::TypePtr>> const& params)
{
	const auto args{ params.select([this](Type::TypePtr const& argType)
	{
		return getCorrespondingType(argType);
	}).Cast<std::vector<llvm::Type*>>() };

	return llvm::FunctionType::get(getCorrespondingType(resultType), args, false);
}

llvm::Type* AotCompiler::buildFunctionType(natRefPointer<Declaration::FunctionDecl> const& funcDecl)
{
	const auto functionType = funcDecl->GetValueType().UnsafeCast<Type::FunctionType>();
	return buildFunctionType(functionType->GetResultType(), functionType->GetParameterTypes());
}

llvm::Type* AotCompiler::buildFunctionType(natRefPointer<Declaration::MethodDecl> const& methodDecl)
{
	const auto functionType = methodDecl->GetValueType().UnsafeCast<Type::FunctionType>();
	const auto classDecl = dynamic_cast<Declaration::ClassDecl*>(Declaration::Decl::CastFromDeclContext(methodDecl->GetContext()));
	assert(classDecl);
	return buildFunctionType(functionType->GetResultType(), from_values({ m_AstContext.GetPointerType(classDecl->GetTypeForDecl()).UnsafeCast<Type::Type>() })
		.concat(functionType->GetParameterTypes()));
}

llvm::Type* AotCompiler::buildClassType(natRefPointer<Declaration::ClassDecl> const& classDecl)
{
	const auto className = classDecl->GetName();
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

	const auto structType = llvm::StructType::create(m_LLVMContext, fieldTypes, llvm::StringRef{ className.data(), className.size() });
	return structType;
}
