set(LLVM_CONFIG_NAMES llvm-config-8.0 llvm-config80 llvm-config-8
                      llvm-config-7.0 llvm-config70 llvm-config-7
                      llvm-config-6.0 llvm-config60 llvm-config-6
                      llvm-config-5.0 llvm-config50
                      llvm-config-4.0 llvm-config40
                      llvm-config-3.9 llvm-config39
                      llvm-config-3.8 llvm-config38
                      llvm-config-3.7 llvm-config37
                      llvm-config)

find_program(LLVM_CONFIG_EXECUTABLE
    DOC "llvm-config executable"
    NAMES ${LLVM_CONFIG_NAMES}
)

execute_process(
    COMMAND "${LLVM_CONFIG_EXECUTABLE}" --bindir
    COMMAND tr "\n" " "
    OUTPUT_VARIABLE LLVM_BIN_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND "${LLVM_CONFIG_EXECUTABLE}" --cppflags
    COMMAND tr "\n" " "
    OUTPUT_VARIABLE LLVM_CXX_FLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND "${LLVM_CONFIG_EXECUTABLE}" --ldflags
    COMMAND tr "\n" " "
    OUTPUT_VARIABLE LLVM_LINK_FLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND "${LLVM_CONFIG_EXECUTABLE}" --libs all --system-libs
    COMMAND tr "\n" " "
    OUTPUT_VARIABLE LLVM_LIBRARIES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LLVM DEFAULT_MSG
    LLVM_BIN_DIR
    LLVM_CXX_FLAGS
    LLVM_LINK_FLAGS
    LLVM_LIBRARIES)

mark_as_advanced(
    LLVM_BIN_DIR
    LLVM_CXX_FLAGS
    LLVM_LINK_FLAGS
    LLVM_LIBRARIES
)
