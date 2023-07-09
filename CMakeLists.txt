cmake_minimum_required(VERSION 3.3)

set(CMAKE_CXX_STANDARD 20)
project(SaboteurToolset VERSION 1.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wl,--no-undefined")

set(BUILD_SHARED_LIBS
    ON
    CACHE BOOL "")
set(BUILD_STATIC_OBJECTS
    OFF
    CACHE BOOL "")
set(NO_OBJECTS
    ON
    CACHE BOOL "" FORCE)

set(TPD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party)

add_subdirectory(${TPD_PATH}/precore)
include(targetex)

add_subdirectory(${TPD_PATH}/precore/spike ${CMAKE_CURRENT_BINARY_DIR}/spike)
add_subdirectory(${TPD_PATH}/precore/gltf)
set_target_properties(spike PROPERTIES OUTPUT_NAME saboteur_toolset)

file(GLOB ZLIB_SOURCE_FILES "${TPD_PATH}/zlib/*.c")

add_library(zlib_obj OBJECT ${ZLIB_SOURCE_FILES})
target_include_directories(zlib_obj PUBLIC ${TPD_PATH}/zlib/)
set_target_properties(zlib_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(common_obj OBJECT src/hashstorage.cpp)
target_include_directories(common_obj PUBLIC include)
set_target_properties(common_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(common_obj precore)

add_spike_subdir(megapack)
add_spike_subdir(tilepack)
add_spike_subdir(loosefiles)
add_spike_subdir(globalmap)
add_spike_subdir(texture)
add_spike_subdir(francemap)
add_spike_subdir(animpack)
add_spike_subdir(mesh)
add_spike_subdir(luapack)
add_subdirectory(hash)
add_spike_subdir(materials)

install(FILES "names.txt" DESTINATION .)
