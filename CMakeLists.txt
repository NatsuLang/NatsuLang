cmake_minimum_required(VERSION 3.8)
project(NatsuLang CXX)

set(${PROJECT_NAME}_EXTERN_DIR "${${PROJECT_NAME}_SOURCE_DIR}/Extern")

option(BUILD_ASTINTERPRETER "Build ASTInterpreter" ON)

add_subdirectory("${${PROJECT_NAME}_EXTERN_DIR}/NatsuLib")
set(NatsuLib_INCLUDE_DIRS ${NatsuLib_SOURCE_DIR}
	CACHE INTERNAL "NatsuLib: Include Directories" FORCE)

add_subdirectory(NatsuLang)
set(NatsuLang_INCLUDE_DIRS "${NatsuLang_SOURCE_DIR}/${PROJECT_NAME}/include"
	CACHE INTERNAL "NatsuLang: Include Directories" FORCE)

if(BUILD_ASTINTERPRETER)
	add_subdirectory("NatsuLang.ASTInterpreter")
endif()
