cmake_minimum_required(VERSION 3.16)
project(mini_compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
find_package(LLVM 14 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

BISON_TARGET(Parser
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp
)

FLEX_TARGET(Lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

add_executable(mini_cc
    src/ast.cpp
    src/sema.cpp
    src/codegen.cpp
    src/driver.cpp
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)

target_include_directories(mini_cc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LLVM_INCLUDE_DIRS}
)

set(LLVM_COMPILE_DEFS ${LLVM_DEFINITIONS})
separate_arguments(LLVM_COMPILE_DEFS)
foreach(LLVM_DEF IN LISTS LLVM_COMPILE_DEFS)
    if(LLVM_DEF MATCHES "^-D(.+)$")
        list(APPEND LLVM_COMPILE_DEFS_CLEAN "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES LLVM_COMPILE_DEFS_CLEAN)
target_compile_definitions(mini_cc PRIVATE ${LLVM_COMPILE_DEFS_CLEAN})

execute_process(
    COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --libs core support target mc asmprinter x86codegen x86desc x86info
    OUTPUT_VARIABLE LLVM_LINK_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --system-libs
    OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(LLVM_LINK_LIBS)
separate_arguments(LLVM_SYSTEM_LIBS)

target_link_libraries(mini_cc PRIVATE ${LLVM_LINK_LIBS} ${LLVM_SYSTEM_LIBS})
