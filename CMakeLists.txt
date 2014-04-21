PROJECT(RadDebugger)
cmake_minimum_required(VERSION 2.8)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SRC_DIR}/cmake/Modules/")

if (WIN32)
    set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../external/windows")
    set(WIN32_PTHREADS_PATH "${CMAKE_PREFIX_PATH}/pthreads.2")
    set(WIN32_PTHREADS_INCLUDE_PATH "${WIN32_PTHREADS_PATH}/include")
    set(WIN32_PTHREADS_LIB_PATH "${WIN32_PTHREADS_PATH}/lib/x86")
endif()

# find_package(LibLZMA REQUIRED)
# find_package(TinyXML REQUIRED)

message("")
message("cmake options:")
message("  -DCMAKE_BUILD_TYPE=[debug|release]: Build debug or release.")
message("  -DVOGLTEST_LOAD_LIBVOGLTRACE=[On|Off]: glitest will load libvogltrace.so manually (don't implictly link against libgl.so).")
message("  -DVOGLTRACE_NO_PUBLIC_EXPORTS=[On|Off]: Don't define any public GL exports in libvogltrace.so.")
message("  -DVOGL_ENABLE_ASSERTS=[On|Off]: Enable assertions in all voglcore builds (including release).")
message("  -DCMAKE_VERBOSE=[On|Off]: Spew cmake project options.")
message("  -DBUILD_X64=[On|Off]: Build 32 or 64-bit.")
message("  -DCLANG_ANALYZE=[On|Off]: Do clang analyze build (will not link).")
message("  -DCLANG_EVERYTHING=[On|Off]: Do clang build with -Weverything.")
message("  -DUSE_TELEMETRY=[On|Off]: Build with Telemetry.")
message("")

#
#  Components to build
#

### All of these must be uncommented to ship.

add_subdirectory(src/voglcore)
### add_subdirectory(src/voglgen)
### add_subdirectory(src/voglcommon)
### add_subdirectory(src/voglreplay)
### add_subdirectory(src/voglbench)
### add_subdirectory(src/voglsyms)
### add_subdirectory(src/vogltrace)
### add_subdirectory(src/vogltest)
### add_subdirectory(src/voglserver)
### add_subdirectory(src/voglcmd)
### add_subdirectory(src/glxspheres)
### add_subdirectory(src/vogleditor)
### add_subdirectory(src/libbacktrace)
### add_subdirectory(src/libbacktrace_test)
