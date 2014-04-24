project(voglgen)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

require_pthreads()

file( GLOB_RECURSE HDRS *.[h|inl] )

set(SPEC_DIR "${CMAKE_SOURCE_DIR}/glspec")
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

# Generate stuff and copy it. Generator must handle copying internally in the future.

add_custom_target(voglgen_make_inc DEPENDS ${PROJECT_NAME})

add_custom_command(TARGET voglgen_make_inc POST_BUILD
                   COMMAND ${PROJECT_NAME} --specdir ${SPEC_DIR} --outinc "${CMAKE_BINARY_DIR}/voglinc" --outlinker "${SRC_DIR}/vogltrace"
)

build_options_finalize()

