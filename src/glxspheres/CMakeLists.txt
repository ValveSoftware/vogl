project(glxspheres)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
require_sdl2()
require_m()
require_gl()
require_glu()


if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-Wno-documentation)
endif()

include_directories(
    ${GL_INCLUDE}
    ${GLU_INCLUDE}
    ${SDL2_INCLUDE}
    )

set(SRC_LIST
    glxspheres.c
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} SDL)

target_link_libraries(${PROJECT_NAME}
    ${CMAKE_DL_LIBS}
    ${M_LIBRARY}
    ${GL_LIBRARY}
    ${GLU_LIBRARY}
    ${SDL2_LIBRARY}
    )

if(NOT MSVC)
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME}
	${X11_X11_LIB}
    )
endif()

build_options_finalize()

