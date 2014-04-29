project(pxfmt)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()

include_directories(
    )

set(SRC_LIST
    ${SRC_LIST}
    pxfmt.cpp
    pxfmt.h
    pxfmt_case_statements.inl
)

add_compiler_flag("-fPIC")

add_library(${PROJECT_NAME} ${SRC_LIST})

build_options_finalize()

