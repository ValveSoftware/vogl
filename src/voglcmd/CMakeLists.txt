include("${SRC_DIR}/build_options.cmake")

project(vogl)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(MySources 
    voglcmd.cpp 
    trace.cpp
    ${SRC_DIR}/common/channel.cpp
    ${SRC_DIR}/common/channelmgr.cpp
    ${SRC_DIR}/common/mtqueue.cpp
    ${SRC_DIR}/common/launchsteamgame.cpp
    ${SRC_DIR}/common/toclientmsg.cpp
    ${SRC_DIR}/common/pinggame.cpp
    ${SRC_DIR}/common/listfiles.cpp
    )

add_compiler_flag("-fPIC")

include_directories(
    ${SRC_DIR}/voglcore
    )

add_executable(
    ${PROJECT_NAME} 
    ${MySources} 
)

target_link_libraries(${PROJECT_NAME}
    pthread
    rt
    voglcore)

build_options_finalize()

