set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS}")

# TODO: 修改为具体文件
file(GLOB SOURCE_FILES
	*.cpp)

file(GLOB HEADER_FILES
    *.h)

add_library("NatsuLang.AOTCompiler" ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories("NatsuLang.AOTCompiler" PUBLIC ${NatsuLib_INCLUDE_DIRS})
target_include_directories("NatsuLang.AOTCompiler" PUBLIC ${NatsuLang_INCLUDE_DIRS})

target_link_libraries("NatsuLang.AOTCompiler" NatsuLang ${LLVM_LIBRARIES})

set_property(TARGET "NatsuLang.AOTCompiler" APPEND_STRING PROPERTY LINK_FLAGS "${LLVM_LINK_FLAGS}")

target_compile_definitions("NatsuLang.AOTCompiler" PUBLIC NATSULIB_UTF8_SOURCE)
