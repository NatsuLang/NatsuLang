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
	Interpreter theInterpreter{ make_ref<natStreamReader<nStrView::UsingStringType>>(make_ref<natFileStream>("DiagIdMap.txt", true, false)), logger };

	try
	{
		if (argc == 1)
		{
			// REPL 模式
			
			while (true)
			{
				try
				{
					console.Write(u8"Fuyu> "_nv);
					const auto line = static_cast<nString>(console.ReadLine());
					if (line.IsEmpty())
					{
						break;
					}

					theInterpreter.Run(line);
				}
				catch (InterpreterException& e)
				{
					logger.LogErr(e.GetDesc());
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
		logger.LogErr(e.GetDesc());
		logger.LogErr(u8"解释器由于未处理的不可恢复的异常而中止运行，请按 Enter 退出程序");
		console.ReadLine();
	}
	catch (std::exception& e)
	{
		logger.LogErr(e.what());
		logger.LogErr(u8"解释器由于未处理的不可恢复的异常而中止运行，请按 Enter 退出程序");
		console.ReadLine();
	}
}
