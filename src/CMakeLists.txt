PROJECT(RadDebugger)
cmake_minimum_required(VERSION 2.8)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

option(HAS_UPDATED_LIBUNWIND "Build using unw_backtrace_skip" FALSE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(LibLZMA REQUIRED)
find_package(TinyXML REQUIRED)

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
message("  -DHAS_UPDATED_LIBUNWIND=[On|Off]: Build using unw_backtrace_skip (from valve patched libunwind)")
message("")

#
#  Components to build
#

add_subdirectory(voglcore)
add_subdirectory(voglgen)
add_subdirectory(voglcommon)
add_subdirectory(voglreplay)
add_subdirectory(voglbench)
add_subdirectory(voglsyms)
add_subdirectory(vogltrace)
add_subdirectory(vogltest)
add_subdirectory(voglserver)
add_subdirectory(voglcmd)
add_subdirectory(glxspheres)
add_subdirectory(vogleditor)
add_subdirectory(libbacktrace)
add_subdirectory(libbacktrace_test)

add_subdirectory(libbacktrace)
add_subdirectory(libbacktrace_test)
