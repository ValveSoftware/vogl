# PROJECT(gltests)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")
include(FindPkgConfig)
pkg_search_module(SDL2 REQUIRED sdl2)
find_package(OpenGL REQUIRED)

# Add the gltest/include directory
include_directories(
    ${SRC_DIR}/gltests/include
    )

#
#  Components to build
#
add_subdirectory(texture-nonzerobase)
add_subdirectory(texture-array-nonzerobase)
add_subdirectory(texture-array-1d)
add_subdirectory(texture-array-2d)
add_subdirectory(texture-array-2d-msaa)
add_subdirectory(texture-array-subimages)
add_subdirectory(core-vao)
add_subdirectory(fixedfunc-multitexcoord-2d)
add_subdirectory(indexed-geometry)
add_subdirectory(benchmark_sphere)
add_subdirectory(msaa_depthstencil)


