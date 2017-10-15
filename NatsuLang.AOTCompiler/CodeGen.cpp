#include "CodeGen.h"

#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>

using namespace NatsuLib;
using namespace NatsuLang;
using namespace Compiler;

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
	for (auto decl : decls)
	{
		if (const auto funcDecl = static_cast<natRefPointer<Declaration::FunctionDecl>>(decl))
		{
			AotStmtVisitor visitor{ m_Compiler, funcDecl };
			visitor.StartVisit();
		}
	}

	return true;
}

AotCompiler::AotStmtVisitor::AotStmtVisitor(AotCompiler& compiler, natRefPointer<Declaration::FunctionDecl> funcDecl)
	: m_Compiler{ compiler }, m_CurrentFunction{ std::move(funcDecl) },
	  m_LastVisitedValue{ nullptr }
{
	const auto functionType = m_Compiler.getCorrespondingType(funcDecl->GetValueType());
	const auto functionName = funcDecl->GetIdentifierInfo()->GetName();

	m_CurrentFunctionValue = llvm::Function::Create(static_cast<llvm::FunctionType*>(functionType),
		// TODO: 修改链接性
		llvm::GlobalVariable::ExternalLinkage,
		std::string(functionName.cbegin(), functionName.cend()),
		m_Compiler.m_Module.get());

	for (auto& arg : from(m_CurrentFunctionValue->args()).zip(funcDecl->GetParams()))
	{
		const auto name = arg.second->GetIdentifierInfo()->GetName();
		arg.first.setName(std::string{ name.cbegin(), name.cend() });
	}

	const auto block = llvm::BasicBlock::Create(m_Compiler.m_LLVMContext, "Entry", m_CurrentFunctionValue);
	m_Compiler.m_IRBuilder.SetInsertPoint(block);
}

AotCompiler::AotStmtVisitor::~AotStmtVisitor()
{
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
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitConditionalOperator(natRefPointer<Expression::ConditionalOperator> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitArraySubscriptExpr(natRefPointer<Expression::ArraySubscriptExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitBinaryOperator(natRefPointer<Expression::BinaryOperator> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCompoundAssignOperator(natRefPointer<Expression::CompoundAssignOperator> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitBooleanLiteral(natRefPointer<Expression::BooleanLiteral> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
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

void AotCompiler::AotStmtVisitor::VisitThisExpr(natRefPointer<Expression::ThisExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitThrowExpr(natRefPointer<Expression::ThrowExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCallExpr(natRefPointer<Expression::CallExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitMemberCallExpr(natRefPointer<Expression::MemberCallExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCastExpr(natRefPointer<Expression::CastExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitAsTypeExpr(natRefPointer<Expression::AsTypeExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitImplicitCastExpr(natRefPointer<Expression::ImplicitCastExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitCharacterLiteral(natRefPointer<Expression::CharacterLiteral> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitDeclRefExpr(natRefPointer<Expression::DeclRefExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitFloatingLiteral(natRefPointer<Expression::FloatingLiteral> const& expr)
{
	const auto floatType = static_cast<natRefPointer<Type::BuiltinType>>(expr->GetExprType());

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
	const auto intType = static_cast<natRefPointer<Type::BuiltinType>>(expr->GetExprType());
	const auto typeInfo = m_Compiler.m_AstContext.GetTypeInfo(intType);

	m_LastVisitedValue = llvm::ConstantInt::get(m_Compiler.m_LLVMContext, llvm::APInt{ typeInfo.Size * 8, expr->GetValue(), intType->IsSigned() });
}

void AotCompiler::AotStmtVisitor::VisitMemberExpr(natRefPointer<Expression::MemberExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitParenExpr(natRefPointer<Expression::ParenExpr> const& expr)
{
	Visit(expr->GetInnerExpr());
}

void AotCompiler::AotStmtVisitor::VisitStmtExpr(natRefPointer<Expression::StmtExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitStringLiteral(natRefPointer<Expression::StringLiteral> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitUnaryExprOrTypeTraitExpr(natRefPointer<Expression::UnaryExprOrTypeTraitExpr> const& expr)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitUnaryOperator(natRefPointer<Expression::UnaryOperator> const& expr)
{
	Visit(expr->GetOperand());

	const auto input = m_LastVisitedValue;
	// TODO: 加载 input
	auto value = input;

	const auto opCode = expr->GetOpcode();

	switch (opCode)
	{
	case Expression::UnaryOperationType::PostInc:
	case Expression::UnaryOperationType::PreInc:
		value = m_Compiler.m_IRBuilder.CreateAdd(value, llvm::ConstantInt::get(value->getType(), 1));
		m_LastVisitedValue = opCode == Expression::UnaryOperationType::PostInc ? value : input;
		break;
	case Expression::UnaryOperationType::PostDec:
	case Expression::UnaryOperationType::PreDec:
		value = m_Compiler.m_IRBuilder.CreateAdd(value, llvm::ConstantInt::get(value->getType(), -1));
		m_LastVisitedValue = opCode == Expression::UnaryOperationType::PostInc ? value : input;
		break;
	case Expression::UnaryOperationType::Plus:
		m_LastVisitedValue = value;
		break;
	case Expression::UnaryOperationType::Minus:
		value = m_Compiler.m_IRBuilder.CreateNeg(value);
		m_LastVisitedValue = value;
		break;
	case Expression::UnaryOperationType::Not:
		value = m_Compiler.m_IRBuilder.CreateNot(value);
		m_LastVisitedValue = value;
		break;
	case Expression::UnaryOperationType::LNot:
		value = m_Compiler.m_IRBuilder.CreateNot(value);
		m_LastVisitedValue = value;
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
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitLabelStmt(natRefPointer<Statement::LabelStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitNullStmt(natRefPointer<Statement::NullStmt> const& stmt)
{
}

void AotCompiler::AotStmtVisitor::VisitReturnStmt(natRefPointer<Statement::ReturnStmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
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
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
}

void AotCompiler::AotStmtVisitor::StartVisit()
{
	Visit(m_CurrentFunction->GetBody());
}

llvm::Function* AotCompiler::AotStmtVisitor::GetFunction() const noexcept
{
	std::string verifyInfo;
	llvm::raw_string_ostream os{ verifyInfo };
	if (verifyFunction(*m_CurrentFunctionValue, &os))
	{
		nat_Throw(AotCompilerException, u8"函数验证错误，信息为 {0}"_nv, verifyInfo);
	}

	return m_CurrentFunctionValue;
}

AotCompiler::AotCompiler(natLog& logger)
	: m_DiagConsumer{ make_ref<AotDiagConsumer>(*this) },
	m_Diag{ make_ref<AotDiagIdMap>(), m_DiagConsumer },
	m_Logger{ logger },
	m_SourceManager{ m_Diag, m_FileManager },
	m_Preprocessor{ m_Diag, m_SourceManager },
	m_Consumer{ make_ref<AotAstConsumer>(*this) },
	m_Sema{ m_Preprocessor, m_AstContext, m_Consumer },
	m_Parser{ m_Preprocessor, m_Sema },
	m_Visitor{ make_ref<AotStmtVisitor>(*this) },
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

	ParseAST(m_Parser);

	llvm::legacy::PassManager passManager;
	machine->addPassesToEmitFile(passManager, stream, llvm::TargetMachine::CGFT_ObjectFile);
	passManager.run(*m_Module);
}

llvm::Type* AotCompiler::getCorrespondingType(Type::TypePtr const& type)
{
	const auto underlyingType = Type::Type::GetUnderlyingType(type);
	const auto typeClass = underlyingType->GetType();

	switch (typeClass)
	{
	case Type::Type::Builtin:
	{
		const auto builtinType = static_cast<natRefPointer<Type::BuiltinType>>(underlyingType);
		switch (builtinType->GetBuiltinClass())
		{
		case Type::BuiltinType::Void:
			return llvm::Type::getVoidTy(m_LLVMContext);
		case Type::BuiltinType::Bool:
			return llvm::Type::getInt1Ty(m_LLVMContext);
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
			return llvm::IntegerType::get(m_LLVMContext, m_AstContext.GetTypeInfo(builtinType).Size * 8);
		case Type::BuiltinType::Float:
			return llvm::Type::getFloatTy(m_LLVMContext);
		case Type::BuiltinType::Double:
			return llvm::Type::getDoubleTy(m_LLVMContext);
		case Type::BuiltinType::LongDouble:
		case Type::BuiltinType::Float128:
			return llvm::Type::getFP128Ty(m_LLVMContext);
		case Type::BuiltinType::Overload:
		case Type::BuiltinType::BoundMember:
		case Type::BuiltinType::BuiltinFn:
		default:
			assert(!"Invalid BuiltinClass");
			[[fallthrough]];
		case Type::BuiltinType::Invalid:
			nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
		}
	}
	case Type::Type::Array:
	{
		const auto arrayType = static_cast<natRefPointer<Type::ArrayType>>(underlyingType);
		return llvm::ArrayType::get(getCorrespondingType(arrayType->GetElementType()), static_cast<std::uint64_t>(arrayType->GetSize()));
	}
	case Type::Type::Function:
	{
		const auto functionType = static_cast<natRefPointer<Type::FunctionType>>(underlyingType);
		const auto args{ functionType->GetParameterTypes().select([this](Type::TypePtr const& argType)
		{
			return getCorrespondingType(argType);
		}).Cast<std::vector<llvm::Type*>>() };

		return llvm::FunctionType::get(getCorrespondingType(functionType->GetResultType()), args, false);
	}
	case Type::Type::Record:
		nat_Throw(AotCompilerException, u8"此功能尚未实现"_nv);
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
}
