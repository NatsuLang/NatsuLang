set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS}")

# TODO: 修改为具体文件
file(GLOB SOURCE_FILES
	*.cpp)

file(GLOB HEADER_FILES
    *.h)

add_executable("NatsuLang.AOTCompiler.Cli" ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories("NatsuLang.AOTCompiler.Cli" PUBLIC ${NatsuLib_INCLUDE_DIRS})
target_include_directories("NatsuLang.AOTCompiler.Cli" PUBLIC ${NatsuLang_INCLUDE_DIRS})
target_include_directories("NatsuLang.AOTCompiler.Cli" PUBLIC ${NatsuLang_AOTCompiler_INCLUDE_DIRS})

target_link_libraries("NatsuLang.AOTCompiler.Cli" "NatsuLang.AOTCompiler")

target_compile_definitions("NatsuLang.AOTCompiler.Cli" PUBLIC NATSULIB_UTF8_SOURCE)

set(DIAGIDMAP_FILE_NAME DiagIdMap.txt)
set(DIAGIDMAP_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${DIAGIDMAP_FILE_NAME}")
add_custom_command(
	TARGET "NatsuLang.AOTCompiler.Cli"
	COMMAND "${CMAKE_COMMAND}" -E copy "${DIAGIDMAP_FILE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${DIAGIDMAP_FILE_NAME}"
	DEPENDS "${DIAGIDMAP_FILE_PATH}"
	COMMENT "Copying DiagIdMap"
	)
