cmake_minimum_required(VERSION 2.8)
project(vogltrace)

option(VOGLTRACE_NO_PUBLIC_EXPORTS "Don't define any public GL exports in libvogltraceXX.so, this .so will only be useful for manual loading" FALSE)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
require_libjpegturbo()
require_app()
request_backtrace()

include_directories(${CMAKE_CURRENT_BINARY_DIR})

# add_compiler_flag("-DVOGL_REMOTING")

set(SRC_LIST
    ${SRC_LIST}
    vogl_trace.cpp
    vogl_intercept.cpp
    # vogl_remote.cpp
    # ${SRC_DIR}/common/channel.cpp
    # ${SRC_DIR}/common/mtqueue.cpp
    # ${SRC_DIR}/common/channelmgr.cpp
    # ${SRC_DIR}/common/toclientmsg.cpp
    # ${SRC_DIR}/common/pinggame.cpp
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH TRUE)
endif()

if (VOGLTRACE_NO_PUBLIC_EXPORTS)
	message("libvogltrace.so will NOT define GL/GLX public exports")
    add_compiler_flag("-DNO_PUBLIC_EXPORTS")
else()
	message("libvogltrace.so will define GL/GLX public exports")
endif()

include_directories(
    ${LibBackTrace_INCLUDE}
    ${SRC_DIR}/extlib/loki/include/loki
    ${SRC_DIR}/voglcommon
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/libtelemetry
    ${LibJpegTurbo_INCLUDE}
)

# Not sure if -Wl,-Bsymbolic is needed or useful?
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  add_shared_linker_flag("-Wl,-Bsymbolic")
  add_shared_linker_flag("-Wl,--version-script=${PROJECT_SOURCE_DIR}/libvogltrace_linker_script.txt")
endif()

if (MSVC)
    if (BUILD_X64)
        set(SRC_LIST ${SRC_LIST} vogl_trace_x64.def)
    else()
        set(SRC_LIST ${SRC_LIST} vogl_trace_x86.def)
    endif()
endif()

add_library(${PROJECT_NAME} SHARED ${SRC_LIST})
add_dependencies(${PROJECT_NAME} voglgen_make_inc)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # Set version to match OpenGL.framework
    set_target_properties (${PROJECT_NAME} PROPERTIES
        SOVERSION "1.0.0"
        VERSION   "1.0.0"
        )
    
    # Set name to match Mac convention
    #
    # The version number comes before the .dylib on the Mac ("foo.1.2.3.dylib").
    #
    # As we also add a "32" or "64" suffix to indicate architecture we insert a hypen
    # to make it clear this isn't part of the version number ("foo.1.2.3-64.dylib").
    set(CMAKE_SHARED_LIBRARY_SUFFIX "-${CMAKE_SHARED_LIBRARY_SUFFIX}")

endif()


target_link_libraries(${PROJECT_NAME}
    ${TELEMETRY_LIBRARY}
    ${LibBackTrace_LIBRARY}
    voglcommon
    voglcore
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${APP_LIBRARY}
    ${LibJpegTurbo_LIBRARY}
)

# Create OpenGL.framework
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD

        COMMENT           "Creating OpenGL.framework"
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"

        COMMAND rm    -fR "OpenGL.framework"
        COMMAND mkdir -p  "OpenGL.framework/Versions/A/Resources"

        COMMAND cp "${PROJECT_SOURCE_DIR}/libvogltrace_framework_plist.txt" "OpenGL.framework/Versions/A/Resources/Info.plist"
        COMMAND ln -sf "$<TARGET_FILE:vogltrace>"                           "OpenGL.framework/Versions/A/OpenGL"
        COMMAND ln -sf "A"                                                  "OpenGL.framework/Versions/Current"
        COMMAND ln -sf "Versions/Current/OpenGL"                            "OpenGL.framework/OpenGL"
        COMMAND ln -sf "Versions/Current/Resources"                         "OpenGL.framework/Resources"
        )

endif()


build_options_finalize()

#
#  Installation
#
install(TARGETS vogltrace
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
