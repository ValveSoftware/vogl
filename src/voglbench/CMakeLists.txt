project(voglbench)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
request_backtrace()

find_package(X11 REQUIRED)

include_directories(
    ${LibBackTrace_INCLUDE}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SRC_DIR}/voglcore
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/voglcommon
    ${SRC_DIR}/libtelemetry
    ${SRC_DIR}/extlib/loki/include/loki
)

set(SRC_LIST
    voglbench.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} voglgen_make_inc)

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    ${LibBackTrace_LIBRARY}
    voglcommon
    ${CMAKE_DL_LIBS}
    ${X11_X11_LIB}
    voglcore
    ${LIBRT}
)

build_options_finalize()

