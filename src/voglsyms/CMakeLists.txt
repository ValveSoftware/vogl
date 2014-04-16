project(voglsyms)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SRC_DIR}/voglcore
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/voglcommon
    ${SRC_DIR}/libtelemetry
    ${SRC_DIR}/libbacktrace
    ${SRC_DIR}/extlib/loki/include/loki
    )

set(SRC_LIST
    voglsyms.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} voglgen_make_inc)

target_link_libraries(${PROJECT_NAME}
    backtracevogl
    voglcommon
    ${CMAKE_DL_LIBS}
    voglcore
    rt
    )

build_options_finalize()

