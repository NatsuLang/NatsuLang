# TODO: 修改为具体文件
file(GLOB_RECURSE SOURCE_FILES
	src/*.cpp)

file(GLOB_RECURSE HEADER_FILES
	include/*.h)

add_library(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${NatsuLib_INCLUDE_DIRS})

set(${PROJECT_NAME}_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/include")
target_include_directories(${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} NatsuLib)

target_compile_definitions(${PROJECT_NAME} PUBLIC NATSULIB_UTF8_SOURCE)
