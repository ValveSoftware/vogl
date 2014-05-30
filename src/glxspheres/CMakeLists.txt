project(glxspheres)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
require_sdl2()


if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-Wno-documentation)
endif()

if (MSVC)
    set(VOGLTEST_OPENGL_LIBRARY
        winmm.lib
        opengl32.lib
        glu32.lib
    )
else()
    set(VOGLTEST_OPENGL_LIBRARY
        m
        GL
        GLU
    )
endif()

include_directories(
    ${SDL2_INCLUDE}
    )

set(SRC_LIST
    glxspheres.c
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} SDL)

target_link_libraries(${PROJECT_NAME}
    ${CMAKE_DL_LIBS}
    ${VOGLTEST_OPENGL_LIBRARY}
    ${SDL2_LIBRARY}
    )

build_options_finalize()

