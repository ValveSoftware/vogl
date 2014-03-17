project(voglgen)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

set(SRC_LIST voglgen.cpp)

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

build_options_finalize()

