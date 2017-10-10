#include "CodeGen.h"

#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Compiler;

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

std::unique_ptr<llvm::Module> AotCompiler::Compile(Uri const& uri)
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
		return nullptr;
	}

	const llvm::TargetOptions opt;
	const llvm::Optional<llvm::Reloc::Model> RM{};
	const auto machine = target->createTargetMachine(targetTriple, "generic", "", opt, RM);
	m_Module->setDataLayout(machine->createDataLayout());

	// TODO
	nat_Throw(NotImplementedException);
}
