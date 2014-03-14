project(glxspheres)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")
find_package(X11 REQUIRED)

aux_source_directory(. SRC_LIST)

include_directories(
	${PROJECT_SOURCE_DIR}/../include
    )

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    ${X11_X11_LIB}
    ${OPENGL_LIBRARY}
    m
    GLU)

build_options_finalize()
