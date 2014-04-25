project(voglgen)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()

file( GLOB_RECURSE HDRS *.[h|inl] )

set(SPEC_DIR "${SRC_DIR}/../glspec")

file( GLOB SPECS "${SPEC_DIR}/*.spec" "${SPEC_DIR}/*.tm" "${SPEC_DIR}/*.xml" )
set(SRC_LIST ${HDRS} ${SPECS} voglgen.cpp)

# This is not the ideal way to include TinyXML, but let's do this for now.
if (NOT TinyXML_FOUND)
    set(SRC_LIST 
        ${SRC_LIST}
        tinyxml/tinystr.cpp
        tinyxml/tinyxml.cpp
        tinyxml/tinyxmlerror.cpp
        tinyxml/tinyxmlparser.cpp
    )
endif()

# Add a source group to easily look at the spec files.
if (MSVC)
    SOURCE_GROUP( "Specs Files" FILES ${SPECS} )
endif()

add_definitions(-DTIXML_USE_STL)

include_directories(
    ${SRC_DIR}/voglcore
    ${TinyXML_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    voglcore
    ${TinyXML_LIBRARIES}
)

set(GENERATOR_INPUT_FILES
    ${SPEC_DIR}/apitrace_gl_param_info.txt
    ${SPEC_DIR}/dbg_enums.txt
    ${SPEC_DIR}/dbg_final_gl_glx_whitelisted_funcs.txt
    ${SPEC_DIR}/dbg_glxext_funcs.txt
    ${SPEC_DIR}/dbg_glx_enums.txt
    ${SPEC_DIR}/dbg_glx_ext_enums.txt
    ${SPEC_DIR}/dbg_glx_funcs.txt
    ${SPEC_DIR}/dbg_glx_typemap.txt
    ${SPEC_DIR}/dbg_gl_all_funcs.txt
    ${SPEC_DIR}/dbg_gl_funcs.txt
    ${SPEC_DIR}/dbg_gl_glx_array_sizes.txt
    ${SPEC_DIR}/dbg_gl_glx_simple_func_macros.txt
    ${SPEC_DIR}/dbg_gl_glx_types.txt
    ${SPEC_DIR}/dbg_gl_typemap.txt
    ${SPEC_DIR}/dbg_gl_xml_funcs.txt
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
    ${SPEC_DIR}/gl_glx_array_sizes.txt
    ${SPEC_DIR}/gl_glx_displaylist_whitelist.txt
    ${SPEC_DIR}/gl_glx_nongenerated_so_export_list.txt
    ${SPEC_DIR}/gl_glx_nullable_funcs.txt
    ${SPEC_DIR}/gl_glx_simple_replay_funcs.txt
    ${SPEC_DIR}/gl_glx_so_export_list.txt
    ${SPEC_DIR}/gl_glx_types.txt
    ${SPEC_DIR}/gl_glx_whitelisted_funcs.txt
)

set(VOGLINCDIR ${CMAKE_BINARY_DIR}/voglinc)
set(VOGLTRACEDIR ${SRC_DIR}/vogltrace)

set(GENERATED_FILES
    # The linker script
    ${VOGLTRACEDIR}/libvogltrace_linker_script.txt

    # All of the generated include files.
    ${VOGLINCDIR}/glx_enums.inc
    ${VOGLINCDIR}/glx_enum_desc.inc
    ${VOGLINCDIR}/glx_ext_desc.inc
    ${VOGLINCDIR}/glx_ext_enums.inc
    ${VOGLINCDIR}/gl_enums.inc
    ${VOGLINCDIR}/gl_enum_desc.inc
    ${VOGLINCDIR}/gl_gets_approx.inc
    ${VOGLINCDIR}/gl_glx_array_size_macros.inc
    ${VOGLINCDIR}/gl_glx_array_size_macros_validator.inc
    ${VOGLINCDIR}/gl_glx_array_size_macro_func_param_indices.inc
    ${VOGLINCDIR}/gl_glx_categories.inc
    ${VOGLINCDIR}/gl_glx_ctypes.inc
    ${VOGLINCDIR}/gl_glx_ctypes_ptr_to_pointee.inc
    ${VOGLINCDIR}/gl_glx_custom_func_handler_validator.inc
    ${VOGLINCDIR}/gl_glx_custom_return_param_array_size_macro_validator.inc
    ${VOGLINCDIR}/gl_glx_func_defs.inc
    ${VOGLINCDIR}/gl_glx_func_descs.inc
    ${VOGLINCDIR}/gl_glx_func_return_param_array_size_macros.inc
    ${VOGLINCDIR}/gl_glx_protos.inc
    ${VOGLINCDIR}/gl_glx_replay_helper_macros.inc
    ${VOGLINCDIR}/gl_glx_simple_replay_funcs.inc    
)

# Generate stuff. The files are automatically written to the specified directories. 
# Note that the dependencies are not automatic, you must update the lists above by hand.
# TODO: Add some logic to glob the files in SPEC_DIR and then compare them with the above list to 
# ensure we catch all the dependencies.

add_custom_command(OUTPUT ${GENERATED_FILES}
                   COMMAND ${PROJECT_NAME} --specdir ${SPEC_DIR} --outinc "${VOGLINCDIR}" --outlinker "${VOGLTRACEDIR}"
                   MAIN_DEPENDENCY ${voglgen_}
                   DEPENDS ${GENERATOR_INPUT_FILES}
                   COMMENT "Running ${PROJECT_NAME} to generate include and linker files for other targets"
)


add_custom_target(voglgen_make_inc
                  ALL DEPENDS ${PROJECT_NAME} ${GENERATED_FILES}
)

build_options_finalize()

