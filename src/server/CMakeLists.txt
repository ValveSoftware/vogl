include("${SRC_DIR}/build_options.cmake")

project(voglserver)

find_package(Protobuf REQUIRED)
find_package(Threads)
if (NOT CMAKE_USE_PTHREADS_INIT)
    message(ERROR "pthread not found")
endif ()

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    )

file(GLOB ProtoFiles "${SRC_DIR}/common/*.proto")
file(GLOB LLDBProtoFiles "${SRC_DIR}/common/lldb/*.proto")
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${ProtoFiles} ${LLDBProtoFiles})

set(MySources 
    dbgsession.cpp 
    gpureplaysession.cpp
    gputracesession.cpp
    lldbsession.cpp 
    master.cpp 
    mastermon.cpp
    network.cpp 
    server.cpp 
    slave.cpp 
    slavemgr.cpp
    slavenotif.cpp
    sysinfosession.cpp 
    ../common/channel.cpp
    ../common/portmgr.cpp
    ../common/mtqueue.cpp
    )

add_compiler_flag("-fPIC")

add_executable(
    ${PROJECT_NAME} 
    ${MySources} 
    ${PROTO_SRCS} 
    ${PROTO_HDRS}
)

target_link_libraries(${PROJECT_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    rt
    libprotobuf.a)

build_options_finalize()

