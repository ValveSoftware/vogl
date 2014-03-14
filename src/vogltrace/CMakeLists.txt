cmake_minimum_required(VERSION 2.8)

option(VOGLTRACE_NO_PUBLIC_EXPORTS "Don't define any public GL exports in libvogltraceXX.so, this .so will only be useful for manual loading" FALSE)

include("${SRC_DIR}/build_options.cmake")
find_package(X11 REQUIRED)

project(vogltrace)

include_directories(${CMAKE_CURRENT_BINARY_DIR})


add_compiler_flag("-DVOGL_REMOTING")

set(SRC_LIST
    ${SRC_LIST}
    vogl_trace.cpp
    vogl_intercept.cpp
    vogl_remote.cpp
    ${SRC_DIR}/common/channel.cpp
    ${SRC_DIR}/common/mtqueue.cpp
    ${SRC_DIR}/common/channelmgr.cpp
    ${SRC_DIR}/common/toclientmsg.cpp
    ${SRC_DIR}/common/pinggame.cpp
)

if (VOGLTRACE_NO_PUBLIC_EXPORTS)
	message("libvogltrace.so will NOT define GL/GLX public exports")
    add_compiler_flag("-DNO_PUBLIC_EXPORTS")
else()
	message("libvogltrace.so will define GL/GLX public exports")
endif()

include_directories(
    ${SRC_DIR}/libbacktrace
    ${SRC_DIR}/extlib/loki/include/loki
    ${SRC_DIR}/voglcommon
    ${SRC_DIR}/voglinc
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/libtelemetry
    )

# Not sure if -Wl,-Bsymbolic is needed or useful?
add_shared_linker_flag("-Wl,-Bsymbolic")
add_shared_linker_flag("-Wl,--version-script=${PROJECT_SOURCE_DIR}/libvogltrace_linker_script.txt")

add_library(${PROJECT_NAME} SHARED ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    backtrace
    voglcommon
    voglcore
    pthread
    ${CMAKE_DL_LIBS}
    ${X11_X11_LIB}
    libturbojpeg.a
    rt
    )

build_options_finalize()

