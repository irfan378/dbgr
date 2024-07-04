cmake_minimum_required(VERSION 3.10)
project(dbgr)

# Define the executable target, including linenoise.c manually
add_executable(MyExecutable src/main.cpp external/linenoise/linenoise.c)

# Include the linenoise directory for header files
target_include_directories(MyExecutable PRIVATE external/linenoise)

# Include the include directory for additional header files
target_include_directories(MyExecutable PRIVATE include)
