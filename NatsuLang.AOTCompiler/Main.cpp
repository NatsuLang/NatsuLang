#include "CodeGen.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

using namespace NatsuLib;
using namespace NatsuLang::Compiler;

int main(int argc, char* argv[])
{
	natConsole console;
	natEventBus event;
	natLog logger{ event };
	logger.UseDefaultAction(console);

	if (argc == 2)
	{
		AotCompiler compiler{ logger };

		Uri uri{ argv[1] };

		std::string outputName{ uri.GetPath().cbegin(), uri.GetPath().cend() };
		outputName += ".obj";

		std::error_code ec;
		llvm::raw_fd_ostream output{ outputName, ec, llvm::sys::fs::F_None };
		
		if (ec)
		{
			logger.LogErr(u8"目标文件无法打开，错误为：{0}"_nv, ec.message());
			return EXIT_FAILURE;
		}

		compiler.Compile(uri, output);
	}
	else
	{
		// TODO
	}

	console.ReadLine();
}
