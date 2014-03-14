cmake_minimum_required (VERSION 2.8)
project (OGLSuperBible)

include("${SRC_DIR}/build_options.cmake")
find_package(X11 REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/OGLSuperBible")

include_directories(
    ../common
    )

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/common SRC_LIST_COMMON)
add_library(sbcommon STATIC ${SRC_LIST_COMMON})

add_definitions(-Wno-unused-variable)
if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wno-unused-but-set-variable)
endif()

macro(DoSampleDir Dir)
    # look for all subdirectories that start with chapter
    file (GLOB chapters "${Dir}/chapter*")

    foreach(chaptername ${chapters})

        # get just the filename part without the extension
        get_filename_component(chapter ${chaptername} NAME_WE)
        # convert chapter_32 to ch32, etc.
        string(REPLACE "chapter_" "ch" chapter ${chapter})

        # look for all directories inside the chapter dir
        file (GLOB samples "${chaptername}/*")
        foreach(sampledir ${samples})

            # add all source dirs in the chapter32/sampleblah dir
            set(SRC_LIST )
            aux_source_directory(${sampledir} SRC_LIST)

            # create name from directory name and append _ch32
            get_filename_component(samplename ${sampledir} NAME_WE)
            set(PROJECT_NAME "${samplename}_${chapter}")

            add_executable("${PROJECT_NAME}" ${SRC_LIST})
            target_link_libraries(${PROJECT_NAME}
                sbcommon
                ${X11_X11_LIB}
                GL
                m
                GLU
                glut
                )

            # look for all .tga files in this sample directory
            file (GLOB textures "${sampledir}/*.tga")
            foreach(texturename ${textures})
                # get basename of .tga file
                get_filename_component(texbasename ${texturename} NAME_WE)
                # copy our stone.tga file to pyramid_ch8_stone.tga, etc.
                set(outfile "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}_${texbasename}.tga")
                add_custom_command(
                    TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${texturename} ${outfile}
                    MAIN_DEPENDENCY ${texturename})
            endforeach()


            # look for the shader directory and copy it if it exists.
            file (GLOB shaders "${sampledir}/shaders")
            foreach(shaderdir ${shaders})
                set(outdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}_shaders")
                add_custom_command(
                    TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory ${shaderdir} ${outdir}
                    MAIN_DEPENDENCY ${outdir})
            endforeach()

            build_options_finalize()

        endforeach()
    endforeach()
endmacro()

DoSampleDir(
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

