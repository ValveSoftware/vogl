project(test-benchmark_sphere)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

find_package(X11 REQUIRED)

aux_source_directory(. SRC_LIST)

add_definitions("-Wno-unused-variable")

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    ${OPENGL_LIBRARIES}
    ${X11_X11_LIB}
    )

build_options_finalize()

