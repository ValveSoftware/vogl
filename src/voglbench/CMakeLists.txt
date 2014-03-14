project(voglbench)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/voglinc
    ${SRC_DIR}/voglcommon
	${SRC_DIR}/libtelemetry
    ${SRC_DIR}/extlib/loki/include/loki
    )

set(SRC_LIST
    voglbench.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    backtrace
    voglcommon
    ${CMAKE_DL_LIBS}
    X11
    voglcore
    rt
    )

build_options_finalize()

