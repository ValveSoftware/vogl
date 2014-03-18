project(glxspheres)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")
find_package(X11 REQUIRED)
find_package(GLU REQUIRED)

set(glxsphere_SRC glxspheres.c)

add_executable(${PROJECT_NAME} ${glxsphere_SRC})

target_link_libraries(${PROJECT_NAME}
    ${X11_X11_LIB}
    ${OPENGL_LIBRARY}
    m
    ${GLU_LIBRARIES})

build_options_finalize()
