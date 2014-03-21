project(voglgen)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

set(SRC_LIST voglgen.cpp)
set(SPEC_DIR "${CMAKE_SOURCE_DIR}/../glspec")

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
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${PROJECT_NAME} --specdir ${SPEC_DIR}
                   COMMAND mkdir -p "${CMAKE_BINARY_DIR}/voglinc/"
                   COMMAND mv "${SPEC_DIR}/*.inc" "${CMAKE_BINARY_DIR}/voglinc/"
                   COMMAND mv "${SPEC_DIR}/libvogltrace_linker_script.txt" "${CMAKE_SOURCE_DIR}/vogltrace/")
add_custom_target(voglgen_make_inc DEPENDS ${PROJECT_NAME})

build_options_finalize()

