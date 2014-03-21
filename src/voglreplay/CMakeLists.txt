project(voglreplay)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")
find_package(Threads)
if (NOT CMAKE_USE_PTHREADS_INIT)
    message(ERROR "pthread not found")
endif ()
find_package(X11 REQUIRED)


include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    )


include_directories(
    ${SRC_DIR}/voglcore
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/voglcommon
	${SRC_DIR}/libtelemetry
	${SRC_DIR}/extlib/loki/include/loki
    )

set(SRC_LIST
    ${SRC_LIST}
    vogl_replay_tool.cpp
#    vogl_remote.cpp
#    ../common/channel.cpp
)

# add_compiler_flag("-DVOGL_REMOTING")

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} voglgen_make_inc)

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    backtrace
    voglcommon
    ${CMAKE_DL_LIBS}
    ${X11_X11_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    voglcore
    rt
    )

build_options_finalize()

