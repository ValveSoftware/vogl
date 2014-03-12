project(voglgen)
cmake_minimum_required(VERSION 2.8)

include("${SRC_DIR}/build_options.cmake")

aux_source_directory(. SRC_LIST)
aux_source_directory(tinyxml SRC_LIST)

include_directories(
    ${SRC_DIR}/voglcore
    )

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    voglcore
    pthread
    )

build_options_finalize()

