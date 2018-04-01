cmake_minimum_required(VERSION 3.8)
project(NatsuLang CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake/Modules")

set(${PROJECT_NAME}_EXTERN_DIR "${${PROJECT_NAME}_SOURCE_DIR}/Extern")

option(BUILD_ASTINTERPRETER "Build ASTInterpreter" ON)
option(BUILD_AOTCOMPILER "Build AotCompiler" OFF)

add_subdirectory("${${PROJECT_NAME}_EXTERN_DIR}/NatsuLib")
set(NatsuLib_INCLUDE_DIRS ${NatsuLib_SOURCE_DIR}
	CACHE INTERNAL "NatsuLib: Include Directories" FORCE)

add_subdirectory(NatsuLang)
set(NatsuLang_INCLUDE_DIRS "${NatsuLang_SOURCE_DIR}/${PROJECT_NAME}/include"
	CACHE INTERNAL "NatsuLang: Include Directories" FORCE)

if(BUILD_ASTINTERPRETER)
	add_subdirectory("NatsuLang.ASTInterpreter")
endif()

if(BUILD_AOTCOMPILER)
	add_subdirectory("NatsuLang.AOTCompiler")
endif()
