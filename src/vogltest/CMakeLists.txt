project(vogltest)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
require_sdl2()

find_package(X11 REQUIRED)

# option(VOGLTEST_LOAD_LIBVOGLTRACE "If enabled vogltest will load libvogltrace.so manually and not implictly link against libgl.so" FALSE)
# message("VOGLTEST_LOAD_LIBGLITRACE: ${VOGLTEST_LOAD_LIBVOGLTRACE}")
# 
# if (NOT VOGLTEST_LOAD_LIBVOGLTRACE)
#     find_library(OPENGL_gl_LIBRARY
#         NAMES GL MesaGL libGL.so.1
#         PATHS /opt/graphics/OpenGL/lib
#         /usr/openwin/lib
#         /usr/shlib
#         /usr/X11R6/lib
#     )
#     if (NOT OPENGL_gl_LIBRARY)
#         message(FATAL_ERROR "Find_package failed to locate libGL.so.")
#     endif()
#     set(VOGLTEST_OPENGL_LIBRARY "${OPENGL_gl_LIBRARY}")
# else()
#     # Don't link with gl library, and define DLOAD_LIBVOGLTRACE=1...
#     set(VOGLTEST_OPENGL_LIBRARY "")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLOAD_LIBVOGLTRACE=1")
# endif()

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
        ${X11_X11_LIB}
        m
        GL
        GLU
    )
endif()

include_directories(
    ${SRC_DIR}/voglcore
    ${SDL2_INCLUDE}
    )

set(SRC_LIST
    vogltest.cpp
    voglcoretest.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} SDL)

target_link_libraries(${PROJECT_NAME}
    ${CMAKE_DL_LIBS}
    ${X11_X11_LIB}
    voglcore
    ${VOGLTEST_OPENGL_LIBRARY}
    ${SDL2_LIBRARY}
    )

build_options_finalize()

