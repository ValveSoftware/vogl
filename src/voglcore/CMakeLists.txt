project(voglcore)
cmake_minimum_required(VERSION 2.8)

option(VOGL_ENABLE_ASSERTS "Enable assertions in all builds (including release)" FALSE)
find_package(Threads)
if (NOT CMAKE_USE_PTHREADS_INIT)
    message(ERROR "pthread not found")
endif ()

include("${SRC_DIR}/build_options.cmake")

include_directories(
    ${LZMA_INCLUDE_DIR}
    ${LZMADEC_INCLUDE_DIR}
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
    vogl_lzma_codec.cpp
    vogl_math.cpp
    vogl_mem.cpp
    vogl_miniz.cpp
    vogl_miniz_zip.cpp
    vogl_miniz_zip_test.cpp
    vogl_mipmapped_texture.cpp
    vogl_pixel_format.cpp
    vogl_platform.cpp
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

if(MSVC)
else()
    add_compiler_flag("-fPIC")
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
    rt
    ${LZMA_LIBRARIES}
    ${LZMADEC_LIBRARIES}
)

build_options_finalize()

