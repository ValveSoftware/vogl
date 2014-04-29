project(ktxtool)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()

find_package(X11 REQUIRED)

include_directories(
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/extlib/pxfmt
    )

include_directories(
    ${SRC_DIR}/voglcore
    ${CMAKE_BINARY_DIR}/voglinc
    )

set(SRC_LIST
    ${SRC_LIST}
    ktxtool.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${X11_X11_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    voglcore
    pxfmt
    )

build_options_finalize()

