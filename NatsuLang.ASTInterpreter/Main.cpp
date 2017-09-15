#include "Interpreter.h"
#include <natConsole.h>

using namespace NatsuLib;
using namespace NatsuLang;

int main(int argc, char* argv[])
{
	if (argc > 2)
	{
		// TODO: 打印用法
		return 0;
	}

	natConsole console;
	natEventBus eventBus;
	natLog logger{ eventBus };

	logger.UseDefaultAction(console);

	// TODO: 添加诊断信息文件
	Interpreter theInterpreter{ nullptr, logger };

	try
	{
		if (argc == 1)
		{
			// REPL 模式
			const auto line = static_cast<nString>(console.ReadLine());
			while (line != "")
			{
				try
				{
					theInterpreter.Run(line);
				}
				catch (InterpreterException& e)
				{
					console.WriteLineErr(e.GetDesc());
					// 吃掉异常，因为在 REPL 模式下此类异常总被视为可恢复的
				}
			}
		}
		else
		{
			// Interpreter 模式
			theInterpreter.Run(Uri{ argv[1] });
		}
	}
	catch (natException& e)
	{
		console.WriteLineErr(e.GetDesc());
	}
	catch (std::exception& e)
	{
		console.WriteLineErr(nStrView{ e.what() });
	}
}
