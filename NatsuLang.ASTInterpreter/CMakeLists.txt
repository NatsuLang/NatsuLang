cmake_minimum_required(VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	if(MSVC_VERSION GREATER_EQUAL "1900")
			include(CheckCXXCompilerFlag)
			CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
			if(_cpp_latest_flag_supported)
					add_compile_options("/std:c++latest")
			endif()
	endif()
endif()

# TODO: 修改为具体文件
file(GLOB SOURCE_FILES
	*.h	
	*.cpp)

add_executable("NatsuLang.ASTInterpreter" ${SOURCE_FILES})

target_include_directories("NatsuLang.ASTInterpreter" PUBLIC ${NatsuLib_INCLUDE_DIRS})
target_include_directories("NatsuLang.ASTInterpreter" PUBLIC ${NatsuLang_INCLUDE_DIRS})

target_link_libraries("NatsuLang.ASTInterpreter" NatsuLang)

target_compile_definitions("NatsuLang.ASTInterpreter" PUBLIC NATSULIB_UTF8_SOURCE)
