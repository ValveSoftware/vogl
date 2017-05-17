project(ktxtool)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
require_app()
require_gl()

include_directories(
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/extlib/pxfmt
    ${SRC_DIR}/extlib/include
    )

include_directories(
    ${SRC_DIR}/voglcore
    ${CMAKE_BINARY_DIR}/voglinc
    )

set(SRC_LIST
    ${SRC_LIST}
    ktxtool.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${APP_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    voglcore
    pxfmt
    )

build_options_finalize()

#
#  Installation
#
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
