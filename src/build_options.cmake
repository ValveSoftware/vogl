#
# cmake -DCMAKE_BUILD_TYPE=Debug ..
#
#   http://www.cmake.org/Wiki/CMake_FAQ
#   http://www.cmake.org/Wiki/CMake_Useful_Variables
#   http://clang.llvm.org/docs/LanguageExtensions.html
#
#   Address Sanitizer: http://clang.llvm.org/docs/AddressSanitizer.html
#                      https://code.google.com/p/address-sanitizer/wiki/AddressSanitizer#Getting_AddressSanitizer
#
cmake_minimum_required(VERSION 2.8)

set(BUILD_X64 "" CACHE STRING "whether to perform 64-bit build: ON or OFF overrides default detection")

option(CMAKE_VERBOSE "Verbose CMake" FALSE)
if (CMAKE_VERBOSE)
    SET(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# With clang: http://clang.llvm.org/docs/AddressSanitizer.html
# With gcc48: http://indico.cern.ch/getFile.py/access?contribId=1&resId=0&materialId=slides&confId=230762
option(WITH_ASAN "Enable address sanitizer" OFF) # gcc4.8+, clang 3.1+

option(WITH_HARDENING "Enable hardening: Compile-time protection against static sized buffer overflows" OFF)
option(USE_TELEMETRY "Build with Telemetry" OFF)

# Unless user specifies BUILD_X64 explicitly, assume native target
if (BUILD_X64 STREQUAL "")
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_X64 "TRUE")
  else()
    set(BUILD_X64 "FALSE")
  endif()
endif()

# Generate bitness suffix to use, but make sure to include the existing suffix (.exe) 
# for platforms that need it (ie, Windows)
if (BUILD_X64)
    set(CMAKE_EXECUTABLE_SUFFIX "64${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "64${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
    set(CMAKE_EXECUTABLE_SUFFIX "32${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "32${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

# Default to release build
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Make sure we're using 64-bit versions of stat, fopen, etc.
# Large File Support extensions:
#   http://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html#Feature-Test-Macros
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES)

# support for inttypes.h macros
add_definitions(-D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS)

if(MSVC)
    set(CMAKE_CXX_FLAGS_LIST "/W3 /D_CRT_SECURE_NO_WARNINGS=1 /DWIN32 /D_WIN32")
    set(CMAKE_CXX_FLAGS_RELEASE_LIST "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG_LIST "/Od /D_DEBUG")
else()
    set(CMAKE_CXX_FLAGS_LIST "-g -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE_LIST "-g -O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG_LIST "-g -O0 -D_DEBUG")
endif()

if(USE_MALLOC)
    set(CMAKE_CXX_FLAGS_LIST "-DVOGL_USE_STB_MALLOC=0")
endif()

set(CLANG_EVERYTHING 0)
if (NOT VOGL_BUILDING_SAMPLES)
    # Samples have tons of shadowing issues. Only add this flag for regular vogl projects.
    set(CLANG_EVERYTHING 1)
endif()

set(OPENGL_LIBRARY "GL")

if (USE_TELEMETRY)
  add_definitions("-DUSE_TELEMETRY")
endif()

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")

  # clang doesn't print colored diagnostics when invoked from Ninja
  if (UNIX AND CMAKE_GENERATOR STREQUAL "Ninja")
      add_definitions ("-fcolor-diagnostics")
  endif()

  if (CLANG_EVERYTHING)
      set(CMAKE_CXX_FLAGS_LIST ${CMAKE_CXX_FLAGS_LIST}
          # "-pedantic"             # Warn on language extensions
          "-Weverything"            # Enable all warnings
          "-fdiagnostics-show-category=name"
          "-Wno-unused-macros"
          "-Wno-padded"
          "-Wno-variadic-macros"
          "-Wno-missing-variable-declarations"
          "-Wno-missing-prototypes"
          "-Wno-sign-conversion"
          "-Wno-conversion"
          "-Wno-cast-align"
          "-Wno-exit-time-destructors"
          "-Wno-documentation-deprecated-sync"
          "-Wno-documentation-unknown-command"

          # TODO: Would be great to start enabling some of these warnings...
          "-Wno-undefined-reinterpret-cast"
          "-Wno-incompatible-pointer-types-discards-qualifiers"
          "-Wno-float-equal"
          "-Wno-unreachable-code"
          "-Wno-weak-vtables"
          "-Wno-extra-semi"
          "-Wno-disabled-macro-expansion"
          "-Wno-format-nonliteral"
          "-Wno-packed"
          "-Wno-c++11-long-long"
          "-Wno-c++11-extensions"
          "-Wno-gnu-anonymous-struct"
          "-Wno-gnu-zero-variadic-macro-arguments"
          "-Wno-nested-anon-types"
          "-Wno-gnu-redeclared-enum"

          "-Wno-pedantic"
          "-Wno-header-hygiene"
          "-Wno-covered-switch-default"
          "-Wno-duplicate-enum"
          "-Wno-switch-enum"
          "-Wno-extra-tokens"

          # This needs to be removed after fixing the VOGL_NO_ATOMICS, etc.
          "-Wno-undef"
          )
  endif()

endif()

if (MSVC)
else()
    if (NOT BUILD_X64)
        set(CMAKE_CXX_FLAGS_LIST "${CMAKE_CXX_FLAGS_LIST} -m32")
    endif()
endif()

function(add_compiler_flag flag)
    set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS}   ${flag}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
endfunction()

function(add_compiler_flag_debug flag)
    set(CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG}   ${flag}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} ${flag}" PARENT_SCOPE)
endfunction()

function(add_compiler_flag_release flag)
    set(CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE}   ${flag}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} ${flag}" PARENT_SCOPE)
endfunction()


function(add_linker_flag flag)
    set(CMAKE_EXE_LINKER_FLAGS   "${CMAKE_EXE_LINKER_FLAGS} ${flag}" PARENT_SCOPE)
endfunction()

function(add_shared_linker_flag flag)
    set(CMAKE_SHARED_LINKER_FLAGS   "${CMAKE_SHARED_LINKER_FLAGS} ${flag}" PARENT_SCOPE)
endfunction()

#
# To show the include files as you're building, do this:
#    add_compiler_flag("-H")
# For Visual Studio, it's /showIncludes I believe...
#

# stack-protector-strong: http://gcc.gnu.org/ml/gcc-patches/2012-06/msg00974.html
## -fstack-protector-strong
# Compile with the option "-fstack-usage" and a file .su will be generated with stack
# information for each function.
## -fstack-usage

# For more info on -fno-strict-aliasing: "Just Say No to strict aliasing optimizations in C": http://nothings.org/
# The Linux kernel is compiled with -fno-strict-aliasing: https://lkml.org/lkml/2003/2/26/158 or http://www.mail-archive.com/linux-btrfs@vger.kernel.org/msg01647.html

### TODO: see if sse is generated with these instructions and clang:
## -march=corei7 -msse -mfpmath=sse

set(MARCH_STR "-march=corei7")
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
   if ( NOT BUILD_X64 )
      # Fix startup crash in dlopen_notify_callback (called indirectly from our dlopen() function) when tracing glxspheres on my AMD dev box (x86 release only)
      # Also fixes tracing Q3 Arena using release tracer
      # Clang is generating sse2 code even when it shouldn't be:
      #  http://lists.cs.uiuc.edu/pipermail/cfe-dev/2012-March/020310.html
      set(MARCH_STR "-march=i586")
   endif()
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS_LIST
        ${CMAKE_CXX_FLAGS_LIST}
        "/EHsc" # Need exceptions
    )
else()
    set(CMAKE_CXX_FLAGS_LIST
        ${CMAKE_CXX_FLAGS_LIST}
        "-fno-omit-frame-pointer"
        ${MARCH_STR}
        # "-msse2 -mfpmath=sse" # To build with SSE instruction sets
        "-Wno-unused-parameter -Wno-unused-function"
        "-fno-strict-aliasing" # DO NOT remove this, we have lots of code that will fail in obscure ways otherwise because it was developed with MSVC first.
        "-fno-math-errno"
    	  "-fvisibility=hidden"
        # "-fno-exceptions" # Exceptions are enabled by default for c++ files, disabled for c files.
    )
endif()

if (CMAKE_COMPILER_IS_GNUCC)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    string(REGEX MATCHALL "[0-9]+" GCC_VERSION_COMPONENTS ${GCC_VERSION})
    list(GET GCC_VERSION_COMPONENTS 0 GCC_MAJOR)
    list(GET GCC_VERSION_COMPONENTS 1 GCC_MINOR)
    # message(STATUS "Detected GCC v ${GCC_MAJOR} . ${GCC_MINOR}")
endif()

if (GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8)
    set(CMAKE_CXX_FLAGS_LIST ${CMAKE_CXX_FLAGS_LIST}
        "-Wno-unused-local-typedefs"
    )
endif()

if (MSVC)
else()
    if (WITH_HARDENING)
        # http://gcc.gnu.org/ml/gcc-patches/2004-09/msg02055.html
        add_definitions(-D_FORTIFY_SOURCE=2 -fpic)
        if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
            # During program load, several ELF memory sections need to be written to by the
            # linker, but can be turned read-only before turning over control to the
            # program. This prevents some GOT (and .dtors) overwrite attacks, but at least
            # the part of the GOT used by the dynamic linker (.got.plt) is still vulnerable.
            add_definitions(-pie -z now -z relro)
        endif()
    endif()
endif()

if (WITH_ASAN)
    # llvm-symbolizer must be on the path. So something like this:
    #  path_prepend /home/mikesart/dev/voglproj/vogl_chroot/vogl_extbuild/i386/clang34_rev1/build/bin
    set(CMAKE_CXX_FLAGS_LIST "${CMAKE_CXX_FLAGS_LIST} -fsanitize=address -fno-optimize-sibling-calls -DVOGL_USE_STB_MALLOC=0")
    set(CMAKE_EXE_LINK_FLAGS_LIST "${CMAKE_EXE_LINK_FLAGS_LIST} -fsanitize=address")
    set(CMAKE_SHARED_LINK_FLAGS_LIST "${CMAKE_SHARED_LINK_FLAGS_LIST} -fsanitize=address")
endif()

if (NOT MSVC)
    set(CMAKE_EXE_LINK_FLAGS_LIST "-Wl,--no-undefined")

    # When linking shared libraries, the AddressSanitizer run-time is not linked,
    #  so -Wl,-z,defs may cause link errors.
    # http://clang.llvm.org/docs/AddressSanitizer.html
    if (NOT WITH_ASAN)
        set(CMAKE_SHARED_LINK_FLAGS_LIST "-Wl,--no-undefined")
    endif()
endif()

# Compiler flags
string(REPLACE ";" " " CMAKE_C_FLAGS              "${CMAKE_CXX_FLAGS_LIST}")
string(REPLACE ";" " " CMAKE_C_FLAGS_RELEASE      "${CMAKE_CXX_FLAGS_RELEASE_LIST}")
string(REPLACE ";" " " CMAKE_C_FLAGS_DEBUG        "${CMAKE_CXX_FLAGS_DEBUG_LIST}")

string(REPLACE ";" " " CMAKE_CXX_FLAGS            "${CMAKE_CXX_FLAGS_LIST}")
string(REPLACE ";" " " CMAKE_CXX_FLAGS_RELEASE    "${CMAKE_CXX_FLAGS_RELEASE_LIST}")
string(REPLACE ";" " " CMAKE_CXX_FLAGS_DEBUG      "${CMAKE_CXX_FLAGS_DEBUG_LIST}")

# Linker flags (exe)
string(REPLACE ";" " " CMAKE_EXE_LINKER_FLAGS     "${CMAKE_EXE_LINK_FLAGS_LIST}")
# Linker flags (shared)
string(REPLACE ";" " " CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINK_FLAGS_LIST}")

# If building from inside chroot project, use vogl_build there.
if (EXISTS "${CMAKE_SOURCE_DIR}/../bin/chroot_build.sh")
    set(RADPROJ_BUILD_DIR ${CMAKE_SOURCE_DIR}/../vogl_build)
else()
    set(RADPROJ_BUILD_DIR ${CMAKE_SOURCE_DIR}/vogl_build)
endif()
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${RADPROJ_BUILD_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RADPROJ_BUILD_DIR}/bin)

# Telemetry setup
function(telemetry_init)
  if (USE_TELEMETRY)
    include_directories("${SRC_DIR}/telemetry/include")

    if (BUILD_X64)
        find_library(RAD_TELEMETRY_LIBRARY libTelemetryX64.link.a HINTS ${SRC_DIR}/telemetry/lib)
        find_library(RAD_TELEMETRY_SO libTelemetryX64.so HINTS ${SRC_DIR}/telemetry/redist)
    else()
        find_library(RAD_TELEMETRY_LIBRARY libTelemetryX86.link.a HINTS ${SRC_DIR}/telemetry/lib)
        find_library(RAD_TELEMETRY_SO libTelemetryX86.so HINTS ${SRC_DIR}/telemetry/redist)
    endif()
  endif()
endfunction()

if (USE_TELEMETRY)
  set(TELEMETRY_LIBRARY telemetry)
else()
  set(TELEMETRY_LIBRARY )
endif()

function(build_options_finalize)
    if (CMAKE_VERBOSE)
        message("  CMAKE_PROJECT_NAME: ${CMAKE_PROJECT_NAME}")
        message("  PROJECT_NAME: ${PROJECT_NAME}")
        message("  BUILD_X64: ${BUILD_X64}")
        message("  BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
        message("  PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")
        message("  CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
        message("  PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
        message("  CMAKE_CURRENT_LIST_FILE: ${CMAKE_CURRENT_LIST_FILE}")
        message("  CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
        message("  CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
        message("  CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
        message("  EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
        message("  SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")
        message("  SHARED_LIBRARY_C_FLAGS: ${CMAKE_SHARED_LIBRARY_C_FLAGS}")
        message("  SHARED_LIBRARY_CXX_FLAGS: ${CMAKE_SHARED_LIBRARY_CXX_FLAGS}")
        message("  SHARED_LIBRARY_LINK_CXX_FLAGS: ${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS}")
        message("  SHARED_LIBRARY_LINK_C_FLAGS: ${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS}")
        message("  CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
        message("  CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
        message("  CMAKE_C_COMPILER_ID: ${CMAKE_C_COMPILER_ID}")
        message("  CMAKE_EXECUTABLE_SUFFIX: ${CMAKE_EXECUTABLE_SUFFIX}")
        message("")
    endif()
endfunction()

function(require_pthreads)
    find_package(Threads)
    if (NOT CMAKE_USE_PTHREADS_INIT AND NOT WIN32_PTHREADS_INCLUDE_PATH)
        message(FATAL_ERROR "pthread not found")
    endif()

    if (MSVC)
        include_directories("${WIN32_PTHREADS_INCLUDE_PATH}")
        if (BUILD_X64)
            set(PTHREAD_SRC_LIB "${WIN32_PTHREADS_PATH}/lib/x64/pthreadVC2.lib" PARENT_SCOPE)
            set(PTHREAD_SRC_DLL "${WIN32_PTHREADS_PATH}/dll/x64/pthreadVC2.dll" PARENT_SCOPE)
        else()
            set(PTHREAD_SRC_LIB "${WIN32_PTHREADS_PATH}/lib/x86/pthreadVC2.lib" PARENT_SCOPE)
            set(PTHREAD_SRC_DLL "${WIN32_PTHREADS_PATH}/dll/x86/pthreadVC2.dll" PARENT_SCOPE)
        endif()

    else()
        # Export the variable to the parent scope so the linker knows where to find the library.
        set(CMAKE_THREAD_LIBS_INIT ${CMAKE_THREAD_LIBS_INIT} PARENT_SCOPE)
    endif()
endfunction()

function(require_libjpegturbo)
    find_library(LibJpegTurbo_LIBRARY
        NAMES libturbojpeg.so libturbojpeg.so.0 libturbojpeg.dylib
        PATHS /opt/libjpeg-turbo/lib 
    )

    # On platforms that find this, the include files will have also been installed to the system
    # so we don't need extra include dirs.
    if (LibJpegTurbo_LIBRARY)
        set(LibJpegTurbo_INCLUDE "" PARENT_SCOPE)
    else()
        if (BUILD_X64)
            set(BITS_STRING "x64")
        else()
            set(BITS_STRING "x86")
        endif()
        set(LibJpegTurbo_INCLUDE "${CMAKE_PREFIX_PATH}/libjpeg-turbo-2.1.3/include" PARENT_SCOPE)
        set(LibJpegTurbo_LIBRARY "${CMAKE_PREFIX_PATH}/libjpeg-turbo-2.1.3/lib_${BITS_STRING}/turbojpeg.lib" PARENT_SCOPE)
    endif()
endfunction()

function(require_sdl2)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        include(FindPkgConfig)
        pkg_search_module(PC_SDL2 REQUIRED sdl2)

        find_path(SDL2_INCLUDE SDL.h
            DOC "SDL2 Include Path"
	    HINTS ${PC_SDL2_INCLUDEDIR} ${PC_SDL2_INCLUDE_DIRS} )

        find_library(SDL2_LIBRARY SDL2
            DOC "SDL2 Library"
	    HINTS ${PC_SDL2_LIBDIR} ${PC_SDL2_LIBRARY_DIRS} )
    elseif (MSVC)
        set(SDL2Root "${CMAKE_EXTERNAL_PATH}/SDL")

        set(SDL2_INCLUDE "${SDL2Root}/include" CACHE PATH "SDL2 Include Path")
        set(SDL2_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/SDL2.lib" CACHE FILEPATH "SDL2 Library")

        # Only want to include this once.
        # This has to go into properties because it needs to persist across the entire cmake run.
        get_property(SDL_PROJECT_ALREADY_INCLUDED 
            GLOBAL 
            PROPERTY SDL_PROJECT_INCLUDED
            )

        if (NOT SDL_PROJECT_ALREADY_INCLUDED)
            INCLUDE_EXTERNAL_MSPROJECT(SDL "${SDL2Root}/VisualC/SDL/SDL_VS2013.vcxproj")
            set_property(GLOBAL
                PROPERTY SDL_PROJECT_INCLUDED "TRUE"
                )
            message("Including SDL_VS2013.vcxproj for you!")
        endif()

    else()
        message(FATAL_ERROR "Need to deal with SDL on non-Windows platforms")
    endif()
endfunction()

function(require_m)
    if (MSVC)
	set(M_LIBRARY "winmm.lib" PARENT_SCOPE)
    else()
	set(M_LIBRARY "m" PARENT_SCOPE)
    endif()
endfunction()

function(require_gl)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	include(FindPkgConfig)
	pkg_search_module(PC_GL QUIET gl)

	find_path(GL_INCLUDE GL/gl.h
	    DOC "OpenGL Include Path"
	    HINTS ${PC_GL_INCLUDEDIR} ${PC_GL_INCLUDE_DIRS} )

	find_library(GL_LIBRARY GL
	    DOC "OpenGL Library"
	    HINTS ${PC_GL_LIBDIR} ${PC_GL_LIBRARY_DIRS} )
    elseif (MSVC)
	set(GL_INCLUDE ""	      CACHE PATH "OpenGL Include Path")
	set(GL_LIBRARY "opengl32.lib" CACHE FILEPATH "OpenGL Library")
    else()
	set(GL_INCLUDE ""   CACHE PATH "OpenGL Include Path")
	set(GL_LIBRARY "GL" CACHE FILEPATH "OpenGL Library")
    endif()
endfunction()

function(require_glu)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	include(FindPkgConfig)
	pkg_search_module(PC_GLU QUIET glu)

	find_path(GLU_INCLUDE GL/glu.h
	    DOC "GLU Include Path"
	    HINTS ${PC_GLU_INCLUDEDIR} ${PC_GLU_INCLUDE_DIRS} )

	find_library(GLU_LIBRARY GLU
	    DOC "GLU Library"
	    HINTS ${PC_GLU_LIBDIR} ${PC_GLU_LIBRARY_DIRS} )
    elseif (MSVC)
	set(GLU_INCLUDE ""	    CACHE PATH "GLU Include Path")
	set(GLU_LIBRARY "glu32.lib" CACHE FILEPATH "GLU Library")
    else()
	set(GLU_INCLUDE ""    CACHE PATH "GLU Include Path")
	set(GLU_LIBRARY "GLU" CACHE FILEPATH "GLU Library")
    endif()
endfunction()

function(request_backtrace)
    if (NOT MSVC)
        set( LibBackTrace_INCLUDE "${SRC_DIR}/libbacktrace" PARENT_SCOPE )
        set( LibBackTrace_LIBRARY "backtracevogl" PARENT_SCOPE )
    else()
        set( LibBackTrace_INCLUDE "" PARENT_SCOPE )
        set( LibBackTrace_LIBRARY "" PARENT_SCOPE )
    endif()
endfunction()

# What compiler toolchain are we building on?
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    add_compiler_flag("-DCOMPILER_GCC=1")
    add_compiler_flag("-DCOMPILER_GCCLIKE=1")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "mingw")
    add_compiler_flag("-DCOMPILER_MINGW=1")
    add_compiler_flag("-DCOMPILER_GCCLIKE=1")
elseif (MSVC)
    add_compiler_flag("-DCOMPILER_MSVC=1")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    add_compiler_flag("-DCOMPILER_CLANG=1")
    add_compiler_flag("-DCOMPILER_GCCLIKE=1")
else()
    message("Compiler is ${CMAKE_C_COMPILER_ID}")
    message(FATAL_ERROR "Compiler unset, build will fail--stopping at CMake time.")
endif()

# Platform specific libary defines.
if (WIN32)
    set( LIBRT "" )
    set( LIBDL "" )

elseif (UNIX)
    set( LIBRT rt )
    set( LIBDL dl )
else()
    message(FATAL_ERROR "Need to determine what the library name for 'rt' is for non-windows, non-unix platforms (or if it's even needed).")
endif()

# What OS will we be running on?
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_compiler_flag("-DPLATFORM_OSX=1")
    add_compiler_flag("-DPLATFORM_POSIX=1")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_compiler_flag("-DPLATFORM_LINUX=1")
    add_compiler_flag("-DPLATFORM_POSIX=1")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_compiler_flag("-DPLATFORM_WINDOWS=1")
else()
    message(FATAL_ERROR "Platform unset, build will fail--stopping at CMake time.")
endif()

# What bittedness are we building?
if (BUILD_X64)
    add_compiler_flag("-DPLATFORM_64BIT=1")
else()
    add_compiler_flag("-DPLATFORM_32BIT=1")
endif()

# Compiler flags for windows.
if (MSVC)
    # Multithreaded compilation is a big time saver.
    add_compiler_flag("/MP")

    # In debug, we use the DLL debug runtime.
    add_compiler_flag_debug("/MDd")

    # And in release we use the DLL release runtime
    add_compiler_flag_release("/MD")

    # x64 doesn't ever support /ZI, only /Zi.
    if (BUILD_X64)
      add_compiler_flag("/Zi")
    else()

      # In debug, get debug information suitable for Edit and Continue
      add_compiler_flag_debug("/ZI")

      # In release, still generate debug information (because not having it is dumb)
      add_compiler_flag_release("/Zi")
    endif()

    # And tell the linker to always generate the file for us.
    add_linker_flag("/DEBUG")
endif()
