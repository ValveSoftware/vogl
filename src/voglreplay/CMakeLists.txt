project(vogl)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
require_sdl2()
require_app()

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

include_directories(
    ${SRC_DIR}/voglcore
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/voglcommon
    ${SRC_DIR}/libtelemetry
    ${SRC_DIR}/extlib/loki/include/loki
    ${SDL2_INCLUDE}
)

set(SRC_LIST
    ${SRC_LIST}
    vogl_replay_tool.cpp
    replay_tool_dump.cpp
    replay_tool_parse.cpp
    replay_tool_info.cpp
    replay_tool_json.cpp
    replay_tool_find.cpp
    replay_tool_compare_hash.cpp
    replay_tool_replay.cpp
    replay_tool_syms.cpp
    replay_tool_trace.cpp
#    vogl_remote.cpp
#    ../common/channel.cpp
)

# add_compiler_flag("-DVOGL_REMOTING")

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} voglgen_make_inc)

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    ${LibBackTrace_LIBRARY}
    voglcommon
    ${CMAKE_DL_LIBS}
    ${APP_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    voglcore
    ${SDL2_LIBRARY}
)

build_options_finalize()

#
#  Installation
#
install(TARGETS vogl
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
