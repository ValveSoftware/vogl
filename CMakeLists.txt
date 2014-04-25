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

if (NOT WIN32)
    find_package(LibLZMA REQUIRED)
endif()

find_package(TinyXML)

message("")
message("cmake options:")
message("  -DCMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}': Build debug or release. (Debug|Release)")
message("  -DVOGLTEST_LOAD_LIBVOGLTRACE='${VOGLTEST_LOAD_LIBVOGLTRACE}': glitest will load libvogltrace.so manually (don't implictly link against libgl.so). (On|Off)")
message("  -DVOGLTRACE_NO_PUBLIC_EXPORTS='${VOGLTRACE_NO_PUBLIC_EXPORTS}': Don't define any public GL exports in libvogltrace.so. (On|Off)")
message("  -DVOGL_ENABLE_ASSERTS='${VOGL_ENABLE_ASSERTS}': Enable assertions in all voglcore builds (including release). (On|Off)")
message("  -DCMAKE_VERBOSE='${CMAKE_VERBOSE}': Spew cmake project options. (On|Off)")
message("  -DBUILD_X64='${BUILD_X64}': Build 32 or 64-bit. (On|Off)")
message("  -DWITH_ASAN='${WITH_ASAN}': Build with Address sanitizer. (On|Off)")
message("  -DUSE_TELEMETRY='${USE_TELEMETRY}': Build with Telemetry. (On|Off)")
message("")

#
#  Components to build
#

### All of these must be included eventually.
add_subdirectory(src/voglcore) # 1
add_subdirectory(src/voglgen) # 2

if (NOT WIN32)
    add_subdirectory(src/voglcommon) # 3
    add_subdirectory(src/voglreplay) # 4
    add_subdirectory(src/voglbench) # 5
    add_subdirectory(src/voglsyms) # 6
    add_subdirectory(src/vogltrace) # 7
    add_subdirectory(src/vogltest) # 8
    add_subdirectory(src/voglserver) # 9
    add_subdirectory(src/voglcmd) # 10 
    add_subdirectory(src/glxspheres) # 11 -- Probably linux only.
    add_subdirectory(src/vogleditor) # 12
    add_subdirectory(src/libbacktrace) # 13
    add_subdirectory(src/libbacktrace_test) # 14
endif()
