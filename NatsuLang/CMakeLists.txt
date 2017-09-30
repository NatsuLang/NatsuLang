
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

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# TODO: 修改为具体文件
file(GLOB SOURCE_FILES
        src/*/*.cpp)

file(GLOB HEADER_FILES
        include/*/*.h)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${NatsuLib_INCLUDE_DIRS})

set(${PROJECT_NAME}_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/include")
target_include_directories(${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} NatsuLib)

target_compile_definitions(${PROJECT_NAME} PUBLIC NATSULIB_UTF8_SOURCE)
