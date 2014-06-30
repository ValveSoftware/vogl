project(pxfmt)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()

include_directories(
    "${SRC_DIR}/extlib/include"
    )

set(SRC_LIST
    ${SRC_LIST}
    pxfmt.cpp
    pxfmt.h
    pxfmt_fxt1.cpp
    pxfmt_s3tc.cpp
    pxfmt_etc.cpp
    pxfmt_internal.h
    pxfmt_internal.cpp
    pxfmt_dlopen.h
    pxfmt_case_statements.inl
    texcompress_etc_tmp.h
)

if (NOT MSVC)
add_compiler_flag("-fPIC")
endif()

add_library(${PROJECT_NAME} ${SRC_LIST})

build_options_finalize()

