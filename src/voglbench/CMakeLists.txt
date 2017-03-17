project(voglbench)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
request_backtrace()
require_sdl2()
require_app()

include_directories(
    ${LibBackTrace_INCLUDE}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SRC_DIR}/voglcore
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/voglcommon
    ${SRC_DIR}/libtelemetry
    ${SRC_DIR}/extlib/loki/include/loki
    ${SDL2_INCLUDE}
)

set(SRC_LIST
    voglbench.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} voglgen_make_inc)
if (TARGET SDL)
    add_dependencies(${PROJECT_NAME} SDL)
endif ()

target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    ${LibBackTrace_LIBRARY}
    voglcommon
    ${CMAKE_DL_LIBS}
    ${APP_LIBRARY}
    voglcore
    ${LIBRT}
    ${SDL2_LIBRARY}
)

build_options_finalize()

#
#  Installation
#
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
