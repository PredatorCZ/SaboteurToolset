project(ModLauncher)

set (CMAKE_SYSTEM_NAME Windows)
set(CMAKE_CXX_STANDARD 20)

# specify the cross compiler
set (CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set (CMAKE_CXX_COMPILER i686-w64-mingw32-g++)

# where is the target environment
set (CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)

# search for programs in the build host directories
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# set the resource compiler
set (CMAKE_RC_COMPILER i686-w64-mingw32-windres)
set (CMAKE_MC_COMPILER i686-w64-mingw32-windmc)

set (CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

set (CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
set (CMAKE_BUILD_TYPE "Release")

add_subdirectory(${TPD_PATH}/detours ${CMAKE_CURRENT_BINARY_DIR}/detours)
add_library(sbmod SHARED library.cpp)
target_link_libraries(sbmod detours)
target_compile_definitions(sbmod PRIVATE UNICODE _UNICODE)
target_include_directories(sbmod PRIVATE ${CMAKE_SOURCE_DIR}/include ${TPD_PATH}/spike/include)



add_executable(mod_saboteur launcher.cpp)
target_link_libraries(mod_saboteur detours)
target_compile_definitions(mod_saboteur PRIVATE UNICODE _UNICODE)
