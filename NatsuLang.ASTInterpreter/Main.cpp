#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;

void PrintException(natLog& logger, std::exception const& e);

void PrintException(natLog& logger, natException const& e)
{
	logger.LogErr(u8"捕获到来自函数 {0}({1}:{2})的异常，描述为 {3}"_nv, e.GetSource(), e.GetFile(), e.GetLine(), e.GetDesc());

#ifdef EnableStackWalker
	logger.LogErr(u8"调用栈为"_nv);

	const auto& stackWalker = e.GetStackWalker();

	for (std::size_t i = 0, count = stackWalker.GetFrameCount(); i < count; ++i)
	{
		const auto& symbol = stackWalker.GetSymbol(i);
#ifdef _WIN32
		logger.LogErr(u8"{3}: (0x%p) {4} (地址：0x%p) (文件 {5}:{6} (地址：0x%p))"_nv, symbol.OriginalAddress, reinterpret_cast<const void*>(symbol.SymbolAddress), reinterpret_cast<const void*>(symbol.SourceFileAddress), i, symbol.SymbolName, symbol.SourceFileName, symbol.SourceFileLine);
#else
		logger.LogErr(u8"0x%p : {1}"_nv, symbol.OriginalAddress, symbol.SymbolInfo);
#endif
	}
#endif

	try
	{
		std::rethrow_if_nested(e);
	}
	catch (natException& inner)
	{
		logger.LogErr(u8"由以下异常引起："_nv);
		PrintException(logger, inner);
	}
	catch (std::exception& inner)
	{
		logger.LogErr(u8"由以下异常引起："_nv);
		PrintException(logger, inner);
	}
}

void PrintException(natLog& logger, std::exception const& e)
{
	logger.LogErr(u8"捕获到异常，描述为 {0}"_nv, e.what());

	try
	{
		std::rethrow_if_nested(e);
	}
	catch (natException& inner)
	{
		logger.LogErr(u8"由以下异常引起："_nv);
		PrintException(logger, inner);
	}
	catch (std::exception& inner)
	{
		logger.LogErr(u8"由以下异常引起："_nv);
		PrintException(logger, inner);
	}
}

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

	Interpreter theInterpreter{ make_ref<natStreamReader<nStrView::UsingStringType>>(make_ref<natFileStream>("DiagIdMap.txt", true, false)), logger };

	theInterpreter.RegisterFunction("Print",
		theInterpreter.GetASTContext().GetBuiltinType(Type::BuiltinType::Void),
		{ theInterpreter.GetASTContext().GetBuiltinType(Type::BuiltinType::Int) },
		[&theInterpreter](std::vector<natRefPointer<Declaration::ValueDecl>> const& args) -> natRefPointer<Declaration::ValueDecl>
		{
			theInterpreter.GetDeclStorage().VisitDeclStorage(args.at(0), [](nInt value)
			{
				std::cout << value << std::endl;
			}, NatsuLang::Detail::Expected<nInt>);

			return nullptr;
		});

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
		PrintException(logger, e);
		logger.LogErr(u8"解释器由于未处理的不可恢复的异常而中止运行，请按 Enter 退出程序");
		console.ReadLine();
	}
	catch (std::exception& e)
	{
		PrintException(logger, e);
		logger.LogErr(u8"解释器由于未处理的不可恢复的异常而中止运行，请按 Enter 退出程序");
		console.ReadLine();
	}
}
