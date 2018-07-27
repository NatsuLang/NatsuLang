set(PrecompiledHeader
	Pch.h)

set(SourceFiles
	Main.cpp)

set(SOURCE_FILES
	${SourceFiles})

add_executable("NatsuLang.ASTInterpreter.Cli" ${SOURCE_FILES})

target_include_directories("NatsuLang.ASTInterpreter.Cli" PUBLIC ${NatsuLib_INCLUDE_DIRS})
target_include_directories("NatsuLang.ASTInterpreter.Cli" PUBLIC ${NatsuLang_INCLUDE_DIRS})
target_include_directories("NatsuLang.ASTInterpreter.Cli" PUBLIC ${NatsuLang_ASTInterpreter_INCLUDE_DIRS})

target_link_libraries("NatsuLang.ASTInterpreter.Cli" "NatsuLang.ASTInterpreter")

target_compile_definitions("NatsuLang.ASTInterpreter.Cli" PUBLIC NATSULIB_UTF8_SOURCE)

# 仅包含 Pch.h，不使用预编译头
if(MSVC)
	foreach(src_file ${SourceFiles})
		set_source_files_properties(
			${src_file}
			PROPERTIES
			COMPILE_FLAGS "/FI${PrecompiledHeader}"
			)
	endforeach()
else()
	foreach(src_file ${SourceFiles})
		set_source_files_properties(
			${src_file}
			PROPERTIES
			COMPILE_FLAGS "-include ${PrecompiledHeader}"
			)
	endforeach()
endif()

set(DIAGIDMAP_FILE_NAME DiagIdMap.txt)
set(DIAGIDMAP_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${DIAGIDMAP_FILE_NAME}")
add_custom_command(
	TARGET "NatsuLang.ASTInterpreter.Cli"
	COMMAND "${CMAKE_COMMAND}" -E copy "${DIAGIDMAP_FILE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${DIAGIDMAP_FILE_NAME}"
	DEPENDS "${DIAGIDMAP_FILE_PATH}"
	COMMENT "Copying DiagIdMap"
	)
