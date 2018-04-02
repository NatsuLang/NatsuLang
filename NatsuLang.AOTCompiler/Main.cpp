#include "CodeGen.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

using namespace NatsuLib;
using namespace NatsuLang::Compiler;

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
	natConsole console;
	natEventBus event;
	natLog logger{ event };
	logger.UseDefaultAction(console);

	try
	{
		if (argc >= 2)
		{
			AotCompiler compiler{ make_ref<natStreamReader<nStrView::UsingStringType>>(make_ref<natFileStream>(u8"DiagIdMap.txt"_nv, true, false)), logger };

			std::vector<Uri> sourceFiles, metadataFiles;

			auto argIter = argv + 1;
			const auto argEnd = argv + argc;
			auto includeImported = false;
			auto isSourceFile = true;
			for (; argIter < argEnd; ++argIter)
			{
				if (nStrView{ *argIter } == u8"-i"_nv)
				{
					includeImported = true;
					continue;
				}

				if (nStrView{ *argIter } == u8"-m"_nv)
				{
					isSourceFile = false;
					continue;
				}

				(isSourceFile ? sourceFiles : metadataFiles).emplace_back(*argIter);
			}

			if (!sourceFiles.empty())
			{
				for (const auto& uri : sourceFiles)
				{
					std::string outputName{ uri.GetPath().cbegin(), uri.GetPath().cend() };
					outputName += ".obj";

					std::error_code ec;
					llvm::raw_fd_ostream output{ outputName, ec, llvm::sys::fs::F_None };

					if (ec)
					{
						logger.LogErr(u8"目标文件无法打开，错误为：{0}"_nv, ec.message());
						return EXIT_FAILURE;
					}

					const auto metadata = make_ref<natFileStream>(uri.GetPath() + u8".meta"_nv, false, true);

					compiler.Compile(uri, from(metadataFiles), output);

					compiler.CreateMetadata(metadata, includeImported);
				}
			}
			else
			{
				compiler.LoadMetadata(from(metadataFiles), false);

				const auto metadata = make_ref<natFileStream>(u8"MergedMetadata.meta"_nv, false, true);
				// 没有源文件时总是输出所有元数据
				compiler.CreateMetadata(metadata, true);
				logger.LogMsg(u8"合并的元数据已存储到文件 MergedMetadata.meta");
			}
		}
		else
		{
			console.WriteLine(u8"Aki 版本 0.1\n"
				"NatsuLang 的 AOT 编译器\n"
				"请将欲编译的源码文件作为第一个命令行参数传入\n"
				"若有需要导入的元数据文件请在 -m 开关之后的参数传入\n"
				"开关 -i 表示输出的元数据文件将会包含导入的元数据，若无源码文件输入则此开关无效，所有元数据将会合并输出\n"
				"例如：\n"
				"\t{0} file:///example.nat -m file:///library.meta\n"
				"其中 \"file:///example.nat\" 是将要编译的源码文件路径，"
				"\"file:///library.meta\" 是将要导入的元数据文件，使用标准 uri 形式表示"_nv, argv[0]);
		}

		console.ReadLine();
	}
	catch (natException& e)
	{
		PrintException(logger, e);
		logger.LogErr(u8"编译器由于未处理的不可恢复的异常而中止运行，请按 Enter 退出程序");
		console.ReadLine();
		return EXIT_FAILURE;
	}
	catch (std::exception& e)
	{
		PrintException(logger, e);
		logger.LogErr(u8"编译器由于未处理的不可恢复的异常而中止运行，请按 Enter 退出程序");
		console.ReadLine();
		return EXIT_FAILURE;
	}
}
