project(voglcommon)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()
request_backtrace()
require_sdl2()

include_directories(
    ${SRC_DIR}/extlib/loki/include/loki
    ${SRC_DIR}/voglcommon
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/libtelemetry
    ${LibBackTrace_INCLUDE}
    ${SDL2_INCLUDE}
)

file( GLOB_RECURSE HDRS *.[h|inl] )

set(SRC_LIST
    ${SRC_LIST}
    ${HDRS}
	gl_types.cpp
    vogl_ctypes.cpp
    vogl_gl_utils.cpp
    vogl_entrypoints.cpp
    vogl_trace_packet.cpp
    vogl_trace_file_reader.cpp
    vogl_trace_file_writer.cpp
    vogl_context_info.cpp
    vogl_blob_manager.cpp
    vogl_texture_state.cpp
    vogl_general_context_state.cpp
    vogl_fbo_state.cpp
    vogl_renderbuffer_state.cpp
    vogl_state_vector.cpp
    vogl_texture_format.cpp
    vogl_sampler_state.cpp
    vogl_buffer_state.cpp
    vogl_query_state.cpp
    vogl_shader_state.cpp
    vogl_program_state.cpp
    vogl_gl_object.cpp
    vogl_gl_state_snapshot.cpp
    vogl_vao_state.cpp
    vogl_sync_object.cpp
    vogl_replay_window.cpp
    vogl_gl_replayer.cpp
    vogl_framebuffer_capturer.cpp
    vogl_material_state.cpp
    vogl_light_state.cpp
    vogl_texenv_state.cpp
    vogl_display_list_state.cpp
    vogl_matrix_state.cpp
    vogl_image_formats.inc
    vogl_common.cpp
    vogl_current_vertex_attrib_state.cpp
    vogl_arb_program_state.cpp
    gl_gets.inc
    vogl_handle_tracker.cpp
    vogl_default_framebuffer_state.cpp
    vogl_default_framebuffer_state.h
    vogl_shader_utils.cpp
    vogl_shader_utils.h
    vogl_msaa_texture.cpp
    vogl_msaa_texture.h
)

if (CMAKE_COMPILER_IS_GNUCC)
    # Variable trackigng causes vogl_entrypoints.cpp to take longer than 20 minutes to compile on
    #  gcc 4.6 and gcc 4.8.
    # See this bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=59868
    set_source_files_properties(vogl_entrypoints.cpp PROPERTIES COMPILE_FLAGS -fno-var-tracking)
endif()

if (NOT MSVC)
    add_compiler_flag("-fPIC")
endif()

add_library(${PROJECT_NAME} ${SRC_LIST})
add_dependencies(${PROJECT_NAME} voglgen_make_inc)

target_link_libraries(${PROJECT_NAME} 
    ${LibBackTrace_LIBRARY}
    ${SDL2_LIBRARY}
)

build_options_finalize()

