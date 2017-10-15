#include "CodeGen.h"

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
		compiler.Compile(Uri{ argv[1] });
	}
}
