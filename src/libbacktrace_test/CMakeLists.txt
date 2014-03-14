project(libbacktrace_test)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

# remove -rdynamic from our shared library (for testing realistic backtraces).
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS )
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS )

# Build a shared library for testing stacks going through those.
add_library( mainlib SHARED mainlib.cpp )

# Build our backtrace test app
include_directories(
    ${SRC_DIR}/libbacktrace
    ${SRC_DIR}/voglcore
    )
set(SRC_LIST
    libbacktrace_test.cpp
)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME}
    backtrace
    pthread
    voglcore
    mainlib
    ${CMAKE_DL_LIBS}
    )

build_options_finalize()

