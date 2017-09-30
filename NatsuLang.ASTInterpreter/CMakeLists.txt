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

set(PrecompiledHeader
	Pch.h)

set(PrecompiledSource
	Pch.cpp)

set(HeaderFiles
	Interpreter.h)

set(SourceFiles
	Interpreter.cpp
	Main.cpp)

set(SOURCE_FILES
	${PrecompiledHeader}
	${PrecompiledSource}
	${HeaderFiles}
	${SourceFiles})

if(MSVC)
	set_source_files_properties(${PrecompiledSource}
		PROPERTIES
		COMPILE_FLAGS "/Yc${PrecompiledHeader}"
		)

	foreach(src_file ${SourceFiles})
		set_source_files_properties(
			${src_file}
			PROPERTIES
			COMPILE_FLAGS "/Yu${PrecompiledHeader} /FI${PrecompiledHeader}"
			)
else()
	set_source_files_properties(${PrecompiledHeader}
		PROPERTIES
		COMPILE_FLAGS
		OBJECT_OUTPUTS "${PrecompiledHeader}.gch"
		)

	foreach(src_file ${SourceFiles})
		set_source_files_properties(
			${src_file}
			PROPERTIES
			COMPILE_FLAGS "-include ${PrecompiledHeader}"
			OBJECT_DEPENDS "${PrecompiledHeader}.gch"
			)
endif()

add_executable("NatsuLang.ASTInterpreter" ${SOURCE_FILES})

target_include_directories("NatsuLang.ASTInterpreter" PUBLIC ${NatsuLib_INCLUDE_DIRS})
target_include_directories("NatsuLang.ASTInterpreter" PUBLIC ${NatsuLang_INCLUDE_DIRS})

target_link_libraries("NatsuLang.ASTInterpreter" NatsuLang)

target_compile_definitions("NatsuLang.ASTInterpreter" PUBLIC NATSULIB_UTF8_SOURCE)
