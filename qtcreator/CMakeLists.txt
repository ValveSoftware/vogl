#
# VoglProj QtCreator cmake source tag file.
#
project(VoglProj)
cmake_minimum_required(VERSION 2.8)

# List of file extensions that we search for.
set(EXTLIST *.i *.sh *.inl *.inc *.txt *.vs *.vp *.frag *.vert *.py *.m *.c* *.h* *.S *.ui)

# Vogl directories.
set(VOGL_DIR 
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/../glspec")

# Check for vogl_chroot project right above us.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/mkvogl.sh")
    set(VOGL_DIR
        ${VOGL_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/../../bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../external/SDL"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../tests")
endif()

message("VOGL_DIR is ${VOGL_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

add_definitions("-DVOGL_USE_LINUX_API=1")
add_definitions("-DSDL_VIDEO_OPENGL_GLX=1")
add_definitions("-DSDL_VIDEO_DRIVER_X11=1")
add_definitions("-DSDL_VIDEO_DRIVER_X11_DYNAMIC=1")
add_definitions("-DVOGL_PLATFORM_HAS_SDL=1")
add_definitions("-DVOGL_PLATFORM_HAS_X11=1")
add_definitions("-DVOGL_PLATFORM_HAS_GLX=1")
add_definitions("-DPLATFORM_POSIX=1")
add_definitions("-DPLATFORM_LINUX=1")

# Create list of vogl directories plus extensions.
set(GLOBSPEC)
foreach(dir ${VOGL_DIR})
    foreach(ext ${EXTLIST})
        list(APPEND GLOBSPEC ${dir}/${ext})
    endforeach()
endforeach()

message("globspec is ${GLOBSPEC}")

# Search for all the files.
file(GLOB_RECURSE vogl_srcs
    ${VOGL_DIR}
    ${GLOBSPEC}
    )

# Macro to remove files based on regex pattern.
macro(RemoveSrcFiles pat)
    set(result)
    foreach(file ${vogl_srcs})
        if(file MATCHES ${pat})
        else()
            list(APPEND result ${file})
        endif()
    endforeach()
    set(vogl_srcs ${result})
endmacro()

# Remove all files under .git and .hg directories.
RemoveSrcFiles("/[.]git/")
RemoveSrcFiles("/[.]hg/")

# Spew out all files we've found.
set(dir_list "")
set(count 0)
foreach(file ${vogl_srcs})
    message("${file}")
    math(EXPR count "${count} + 1")
    GET_FILENAME_COMPONENT(dir_path ${file} PATH)
    SET(dir_list ${dir_list} ${dir_path})
endforeach()

list(REMOVE_DUPLICATES dir_list)
include_directories(${dir_list})

message("${count} files added.\n")

add_executable(VoglProj ${vogl_srcs})
set_target_properties(VoglProj PROPERTIES LINKER_LANGUAGE CXX)

