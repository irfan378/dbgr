cmake_minimum_required (VERSION 3.0)
project (dbgr)

add_compile_options(-std=c++14)

include_directories(external/libelfin external/linenoise include)
add_executable(minidbg src/main.cpp external/linenoise/linenoise.c)




add_custom_target(
   libelfin
   COMMAND make
   WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/external/libelfin
)
target_link_libraries(minidbg
                      ${PROJECT_SOURCE_DIR}/external/libelfin/dwarf/libdwarf++.so
                      ${PROJECT_SOURCE_DIR}/external/libelfin/elf/libelf++.so)
add_dependencies(minidbg libelfin)
