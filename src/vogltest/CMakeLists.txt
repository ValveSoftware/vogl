project(vogltest)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")
find_package(X11 REQUIRED)

option(VOGLTEST_LOAD_LIBVOGLTRACE "If enabled vogltest will load libvogltrace.so manually and not implictly link against libgl.so" FALSE)

message("VOGLTEST_LOAD_LIBGLITRACE: ${VOGLTEST_LOAD_LIBVOGLTRACE}")

if (NOT VOGLTEST_LOAD_LIBVOGLTRACE)
    # Link with libGL.so
    set(VOGLTEST_OPENGL_LIBRARY "GL")
else()
    # Don't link with gl library, and define DLOAD_LIBVOGLTRACE=1...
    set(VOGLTEST_OPENGL_LIBRARY "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLOAD_LIBVOGLTRACE=1")
endif()

aux_source_directory(. SRC_LIST)

include_directories(
    ${SRC_DIR}/gltests/include
    ${SRC_DIR}/voglcore
    )

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    voglcore
    ${X11_X11_LIB}
    ${VOGLTEST_OPENGL_LIBRARY}
    ${CMAKE_DL_LIBS}
    )

build_options_finalize()

