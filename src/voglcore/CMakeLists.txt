project(voglcore)
cmake_minimum_required(VERSION 2.8)

option(VOGL_ENABLE_ASSERTS "Enable assertions in all builds (including release)" FALSE)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()

include_directories(
   "${SRC_DIR}/chromiumlib"
)

file( GLOB_RECURSE HDRS *.[h|inl] )

set(SRC_LIST
    ${SRC_LIST}
    ${HDRS}
    vogl_assert.cpp
    vogl_checksum.cpp
    vogl_colorized_console.cpp
    vogl_command_line_params.cpp
    vogl_console.cpp
    vogl_core.cpp
    vogl_data_stream.cpp
    vogl_dxt1.cpp
    vogl_dxt5a.cpp
    vogl_dxt.cpp
    vogl_dxt_fast.cpp
    vogl_dxt_image.cpp
    vogl_dynamic_module.cpp
    vogl_dynamic_string.cpp
    vogl_file_utils.cpp
    vogl_find_files.cpp
    vogl_hash.cpp
    vogl_hash_map.cpp
    vogl_image_utils.cpp
    vogl_jpgd.cpp
    vogl_jpge.cpp
    vogl_json.cpp
    vogl_ktx_texture.cpp
    vogl.cpp
    vogl_math.cpp
    vogl_mem.cpp
    vogl_miniz.cpp
    vogl_miniz_zip.cpp
    vogl_miniz_zip_test.cpp
    vogl_mipmapped_texture.cpp
    vogl_pixel_format.cpp
    vogl_platform.cpp
    vogl_port.cpp
    vogl_rand.cpp
    vogl_resample_filters.cpp
    vogl_resampler.cpp
    vogl_ryg_dxt.cpp
    vogl_sparse_bit_array.cpp
    vogl_stb_image.cpp
    vogl_strutils.cpp
    vogl_texture_file_types.cpp
    vogl_threaded_resampler.cpp
    vogl_threading_pthreads.cpp
    vogl_timer.cpp
    vogl_utils.cpp
    vogl_value.cpp
    vogl_vector.cpp
    vogl_etc.cpp
    vogl_rg_etc1.cpp
    rmalloc.c
    regex/debug.c
    regex/regcomp.c
    regex/regerror.c
    regex/regexec.c
    regex/regfree.c
    vogl_regex.cpp
    vogl_map.cpp
    vogl_md5.cpp
    vogl_introsort.cpp
    vogl_uuid.cpp
    vogl_backtrace.cpp
    stb_malloc.cpp
    vogl_rh_hash_map.cpp
    vogl_object_pool.cpp
)

# Platform specific compile flags.
if (NOT MSVC)
    add_compiler_flag("-fPIC")
endif()

# Platform specific source files.
if (WIN32)
    set(SRC_LIST
        ${SRC_LIST}
        vogl_port_windows.cpp
    )
elseif (UNIX)
    set(SRC_LIST
        ${SRC_LIST}
        vogl_port_posix.cpp
    )
else()
    message(FATAL_ERROR "Need to add a vogl_port_PLATFORM.cpp file for this platform.")
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

if (VOGL_ENABLE_ASSERTS)
    message("Forcing assertions to be enabled (VOGL_ENABLE_ASSERTS=TRUE)")
    add_compiler_flag("-DVOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS=1")
else()
    message("Not forcing assertions to be enabled (VOGL_ENABLE_ASSERTS=FALSE)")
endif()

add_library(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIBRT}
    ${LIBDL}
)

if (MSVC)
    set (PTHREAD_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR})

    add_custom_target(copy_deps ALL
                      ${CMAKE_COMMAND} -E make_directory ${PTHREAD_DEST}
                      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PTHREAD_SRC_DLL} ${PTHREAD_DEST}
                      COMMENT "Copying necessary DLLs to output directory."
                      VERBATIM
    )
endif()

build_options_finalize()

