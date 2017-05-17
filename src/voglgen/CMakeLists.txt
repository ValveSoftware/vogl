project(voglgen)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()

file( GLOB_RECURSE HDRS *.[h|inl] )

set(SPEC_DIR "${SRC_DIR}/../glspec")

file( GLOB SPECS "${SPEC_DIR}/*.spec" "${SPEC_DIR}/*.tm" "${SPEC_DIR}/*.xml" )
set(SRC_LIST ${HDRS} ${SPECS}
    voglgen.cpp
    tinyxml2/tinyxml2.cpp)

# Add a source group to easily look at the spec files.
if (MSVC)
    SOURCE_GROUP( "Specs Files" FILES ${SPECS} )
endif()

add_definitions(-DTIXML_USE_STL)

include_directories(
    ${SRC_DIR}/voglcore
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    voglcore
)

set(FORCE_REGENERATE ${SPEC_DIR}/force_regenerate)


set(GENERATOR_INPUT_FILES
    # A file used to force regeneration of generated files.
    ${FORCE_REGENERATE}

    # The actual input specs.
    ${SPEC_DIR}/apitrace_gl_param_info.txt
    ${SPEC_DIR}/enum.spec
    ${SPEC_DIR}/enumext.spec
    ${SPEC_DIR}/gl.spec
    ${SPEC_DIR}/gl.tm
    ${SPEC_DIR}/gl.xml
    ${SPEC_DIR}/gl10_gets.txt
    ${SPEC_DIR}/gl15_gets.txt
    ${SPEC_DIR}/gl21_gets.txt
    ${SPEC_DIR}/gl33_gets.txt
    ${SPEC_DIR}/gl40_gets.txt
    ${SPEC_DIR}/gl41_gets.txt
    ${SPEC_DIR}/gl42_gets.txt
    ${SPEC_DIR}/gl43_gets.txt
    ${SPEC_DIR}/glx.spec
    ${SPEC_DIR}/glx.tm
    ${SPEC_DIR}/glxenum.spec
    ${SPEC_DIR}/glxenumext.spec
    ${SPEC_DIR}/glxext.spec
    ${SPEC_DIR}/gl_glx_displaylist_whitelist.txt
    ${SPEC_DIR}/gl_glx_cgl_wgl_nongenerated_so_export_list.txt
    ${SPEC_DIR}/gl_glx_nullable_funcs.txt
    ${SPEC_DIR}/gl_glx_simple_replay_funcs.txt
    ${SPEC_DIR}/gl_glx_so_export_list.txt
    ${SPEC_DIR}/gl_glx_whitelisted_funcs.txt

    ${SPEC_DIR}/cgl.spec
    ${SPEC_DIR}/cgl.tm
    ${SPEC_DIR}/cglenum.spec
    ${SPEC_DIR}/cglenumext.spec
    ${SPEC_DIR}/cglext.spec

    ${SPEC_DIR}/gl_cgl_displaylist_whitelist.txt
    ${SPEC_DIR}/gl_cgl_nullable_funcs.txt
    ${SPEC_DIR}/gl_cgl_simple_replay_funcs.txt
    ${SPEC_DIR}/gl_cgl_dylib_export_list.txt
    ${SPEC_DIR}/gl_cgl_whitelisted_funcs.txt

    ${SPEC_DIR}/wgl.spec
    ${SPEC_DIR}/wgl.tm
    ${SPEC_DIR}/wglenum.spec
    ${SPEC_DIR}/wglenumext.spec
    ${SPEC_DIR}/wglext.spec

    ${SPEC_DIR}/gl_wgl_displaylist_whitelist.txt
    ${SPEC_DIR}/gl_wgl_nullable_funcs.txt
    ${SPEC_DIR}/gl_wgl_simple_replay_funcs.txt
    ${SPEC_DIR}/gl_wgl_dll_export_list.txt
    ${SPEC_DIR}/gl_wgl_whitelisted_funcs.txt
)

set(VOGLINCDIR ${CMAKE_BINARY_DIR}/voglinc)
set(VOGLTRACEDIR ${SRC_DIR}/vogltrace)

set(GENERATED_FILES
    # The linker script
    ${VOGLTRACEDIR}/libvogltrace_linker_script.txt

    # All of the generated include files.
    ${VOGLINCDIR}/gl_glx_cgl_wgl_array_size_macros.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_array_size_macros_validator.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_array_size_macro_func_param_indices.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_categories.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_ctypes.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_ctypes_ptr_to_pointee.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_custom_func_handler_validator.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_custom_return_param_array_size_macro_validator.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_func_defs.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_func_descs.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_func_return_param_array_size_macros.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_protos.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_replay_helper_macros.inc
    ${VOGLINCDIR}/gl_glx_cgl_wgl_simple_replay_funcs.inc    

    # platform independent files
    ${VOGLINCDIR}/gl_enums.inc
    ${VOGLINCDIR}/gl_enum_desc.inc
    ${VOGLINCDIR}/gl_gets_approx.inc

    # GLX specific files
    ${VOGLINCDIR}/glx_enums.inc
    ${VOGLINCDIR}/glx_enum_desc.inc
    ${VOGLINCDIR}/glx_ext_desc.inc
    ${VOGLINCDIR}/glx_ext_enums.inc

	# CGL specific files
    ${VOGLINCDIR}/cgl_enums.inc
    ${VOGLINCDIR}/cgl_enum_desc.inc
    ${VOGLINCDIR}/cgl_ext_desc.inc
    ${VOGLINCDIR}/cgl_ext_enums.inc

    # And the wgl specific ones
    ${VOGLINCDIR}/wgl_enums.inc
    ${VOGLINCDIR}/wgl_enum_desc.inc
    ${VOGLINCDIR}/wgl_ext_desc.inc
    ${VOGLINCDIR}/wgl_ext_enums.inc
)

# Generate stuff. The files are automatically written to the specified directories. 
# Note that the dependencies are not automatic, you must update the lists above by hand.
# TODO: Add some logic to glob the files in SPEC_DIR and then compare them with the above list to 
# ensure we catch all the dependencies.

# Don't we want this FORCE_REGENERATE file to be in the build output directory?

# This case deals with rebuilding the exe.
add_custom_command(TARGET ${PROJECT_NAME}
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E touch "${FORCE_REGENERATE}"
)

# And this deals with the input files changing. It's not clear why Cmake can't tolerate this project being
# pushed into the depends list (or listed as the MAIN_DEPENDENCY), but this works so we'll live with it.
add_custom_command(OUTPUT ${GENERATED_FILES}
                   COMMAND $<TARGET_FILE:${PROJECT_NAME}> --specdir ${SPEC_DIR} --outinc "${VOGLINCDIR}" --outlinker "${VOGLTRACEDIR}"
                   DEPENDS ${GENERATOR_INPUT_FILES}
                   COMMENT "Running ${PROJECT_NAME} to generate include and linker files for other targets"
                   VERBATIM
)

add_custom_target(voglgen_make_inc
                  ALL 
                  DEPENDS voglgen ${GENERATED_FILES}
)

if (MSVC)
    add_dependencies(voglgen_make_inc copy_deps)
endif()

build_options_finalize()

#
#  Installation
#
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
