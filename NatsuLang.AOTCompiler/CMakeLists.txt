
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

find_package(LLVM)

if(NOT LLVM_FOUND)
    message(FATAL_ERROR "LLVM not found, aborting.")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS}")

# TODO: 修改为具体文件
file(GLOB SOURCE_FILES
	*.cpp)

file(GLOB HEADER_FILES
    *.h)

add_executable("NatsuLang.AOTCompiler" ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories("NatsuLang.AOTCompiler" PUBLIC ${NatsuLib_INCLUDE_DIRS})
target_include_directories("NatsuLang.AOTCompiler" PUBLIC ${NatsuLang_INCLUDE_DIRS})

target_link_libraries("NatsuLang.AOTCompiler" NatsuLang ${LLVM_LIBRARIES})

set_property(TARGET "NatsuLang.AOTCompiler" APPEND_STRING PROPERTY LINK_FLAGS "${LLVM_LINK_FLAGS}")

target_compile_definitions("NatsuLang.AOTCompiler" PUBLIC NATSULIB_UTF8_SOURCE)

set(DIAGIDMAP_FILE_NAME DiagIdMap.txt)
set(DIAGIDMAP_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${DIAGIDMAP_FILE_NAME}")
add_custom_command(
	TARGET "NatsuLang.AOTCompiler"
	COMMAND "${CMAKE_COMMAND}" -E copy "${DIAGIDMAP_FILE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${DIAGIDMAP_FILE_NAME}"
	DEPENDS "${DIAGIDMAP_FILE_PATH}"
	COMMENT "Copying DiagIdMap"
	)
