
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        if(MSVC_VERSION GREATER_EQUAL "1900")
                include(CheckCXXCompilerFlag)
                CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
                if(_cpp_latest_flag_supported)
                        add_compile_options("/std:c++latest")
                endif()
        endif()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX /wd4100 /wd4996")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused -Werror")
endif()

set(PrecompiledHeader
	Pch.h)

set(PrecompiledSource
	Pch.cpp)

set(HeaderFiles
	Interpreter.h)

set(SourceFiles
	ASTConsumer.cpp
	DeclStorage.cpp
	DiagConsumer.cpp
	DiagIdMap.cpp
	ExprVisitor.cpp
	Interpreter.cpp
	Main.cpp
	StmtVisitor.cpp)

set(SOURCE_FILES
	${PrecompiledHeader}
	${PrecompiledSource}
	${HeaderFiles}
	${SourceFiles})

add_executable("NatsuLang.ASTInterpreter" ${SOURCE_FILES})

target_include_directories("NatsuLang.ASTInterpreter" PUBLIC ${NatsuLib_INCLUDE_DIRS})
target_include_directories("NatsuLang.ASTInterpreter" PUBLIC ${NatsuLang_INCLUDE_DIRS})

target_link_libraries("NatsuLang.ASTInterpreter" NatsuLang)

target_compile_definitions("NatsuLang.ASTInterpreter" PUBLIC NATSULIB_UTF8_SOURCE)

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
	endforeach()
else()
	set(PCH_PATH "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledHeader}.gch")

	# TODO: 替换掉脏实现
	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledHeader}"
		COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${PrecompiledHeader}" "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledHeader}"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PrecompiledHeader}"
		COMMENT "Copying pch"
		)

	add_custom_command(
		OUTPUT "${PCH_PATH}"
		COMMAND "${CMAKE_CXX_COMPILER}" -std=gnu++17 -I${NatsuLib_INCLUDE_DIRS} -I${NatsuLang_INCLUDE_DIRS} -x c++-header -o "${PCH_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledHeader}"
		DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledHeader}"
		COMMENT "Generating gch"
		)

	foreach(src_file ${SourceFiles})
		set_source_files_properties(
			${src_file}
			PROPERTIES
			COMPILE_FLAGS "-include ${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledHeader}"
			OBJECT_DEPENDS "${PCH_PATH}"
			)
	endforeach()
endif()

set(DIAGIDMAP_FILE_NAME DiagIdMap.txt)
set(DIAGIDMAP_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${DIAGIDMAP_FILE_NAME}")
add_custom_command(
	TARGET "NatsuLang.ASTInterpreter"
	COMMAND "${CMAKE_COMMAND}" -E copy "${DIAGIDMAP_FILE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${DIAGIDMAP_FILE_NAME}"
	DEPENDS "${DIAGIDMAP_FILE_PATH}"
	COMMENT "Copying DiagIdMap"
	)
