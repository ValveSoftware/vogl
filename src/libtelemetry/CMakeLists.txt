#
# libtelemetry
#
project(telemetry)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

if (USE_TELEMETRY)

telemetry_init()
find_package(Threads REQUIRED)

include_directories(
    ${SRC_DIR}/voglcore
    )

# macro(remove_cxx_flag flag)
#  string(REPLACE "${flag}" "" CMAKE_SHARED_LIBRARY_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS}")
#  string(REPLACE "${flag}" "" SHARED_LIBRARY_LINK_CXX_FLAGS "${SHARED_LIBRARY_LINK_CXX_FLAGS}")
# endmacro()
#
# add_compiler_flag("-fPIC")
# remove_cxx_flag("-fPIC")
# add_compiler_flag("-mcmodel=large")
# add_shared_linker_flag("-Wl,--version-script=${PROJECT_SOURCE_DIR}/linker-script.txt")

add_library(${PROJECT_NAME} SHARED
    libtelemetry.cpp
    )

target_link_libraries(${PROJECT_NAME}
    ${RAD_TELEMETRY_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    voglcore)

add_custom_target(copy_telemetry_so)
add_custom_command(
    TARGET copy_telemetry_so POST_BUILD
    # COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/..
    COMMAND ${CMAKE_COMMAND} -E copy ${RAD_TELEMETRY_SO} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
message("${PROJECT_NAME} copy ${RAD_TELEMETRY_SO} to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Copy the telemetry shared object over to our bin directory.
add_dependencies(${PROJECT_NAME} copy_telemetry_so)

build_options_finalize()

endif()

