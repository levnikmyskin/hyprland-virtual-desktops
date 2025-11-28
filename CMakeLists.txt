cmake_minimum_required(VERSION 3.27)

project(hyprland-virtual-desktops
    DESCRIPTION "Virtual desktop like workspaces"
    VERSION 2.2.7
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(-Wall -fPIC --no-gnu-unique)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
else()
    add_compile_options(-O2)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB_RECURSE SRC "src/*.cpp")

add_library(hyprland-virtual-desktops SHARED ${SRC})
target_include_directories(hyprland-virtual-desktops PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    libdrm
    pixman-1
)
target_link_libraries(hyprland-virtual-desktops PRIVATE rt PkgConfig::deps)

install(TARGETS hyprland-virtual-desktops)

enable_testing()
add_executable(focus_memory_test tests/focus_memory_test.cpp)
target_include_directories(focus_memory_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME focus_memory_test COMMAND focus_memory_test)

add_executable(utils_test tests/utils_test.cpp)
target_include_directories(utils_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME utils_test COMMAND utils_test)
