cmake_minimum_required(VERSION 3.20)
project(luajit-decompiler-v2 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(luajit-decompiler-v2
    main.cpp
    ast/ast.cpp
    bytecode/bytecode.cpp
    bytecode/prototype.cpp
    lua/lua.cpp
)

target_compile_options(luajit-decompiler-v2 PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-funsigned-char>
    $<$<CXX_COMPILER_ID:MSVC>:/J>
)

if(UNIX AND NOT APPLE)
    target_link_libraries(luajit-decompiler-v2 PRIVATE stdc++fs)
endif()
