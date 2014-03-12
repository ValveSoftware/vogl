project(voglsyms)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/voglinc
    ${SRC_DIR}/voglcommon
	${SRC_DIR}/libtelemetry
	${SRC_DIR}/libbacktrace
    ${SRC_DIR}/extlib/loki/include/loki
    )

set(SRC_LIST
    voglsyms.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    backtrace
    voglcommon
    dl
    pthread
    voglcore
    rt
    )

build_options_finalize()

