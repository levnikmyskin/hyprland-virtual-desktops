cmake_minimum_required(VERSION 3.10)
project(virtual-desktops)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIXMAN REQUIRED pixman-1)
pkg_check_modules(DRM REQUIRED libdrm)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ./include
    ${PIXMAN_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PIXMAN_LIBRARIES}
    ${DRM_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON # -fPIC
    COMPILE_FLAGS "--no-gnu-unique -std=c++23 -Wall"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WLR_USE_UNSTABLE
)

