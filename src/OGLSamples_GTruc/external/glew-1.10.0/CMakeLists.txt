# TODO: Add proper support for OSX.
# GLEW_REGAL
# GLEW_APPLE_GLX
# GLEW_MX
cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
project(GLEW)
find_package(OpenGL REQUIRED)
include_directories( ${CMAKE_SOURCE_DIR}/include ${OPENGL_INCLUDE_DIRS})

option(GLEW_ENABLE_MX
	"Compile GLEWmx Lib" OFF)
option(GLEW_BUILD_EXAMPLES
 "Compile GLEW Samples (glewinfo and visualinfo)" OFF)
option(GLEW_INSTALL
	 "Install GLEW " OFF)
# GLEW 
add_library(GLEW STATIC src/glew.c)
set_target_properties(GLEW PROPERTIES DEFINE_SYMBOL "GLEW_STATIC")
target_link_libraries(GLEW ${OPENGL_LIBRARIES})
if(GLEW_INSTALL)
	install(TARGETS GLEW ARCHIVE DESTINATION .)
endif()
list(APPEND glew_LIBRARIES GLEW ${OPENGL_LIBRARIES})

set(GLEW_LIBRARIES ${glew_LIBRARIES} CACHE STRING "Dependencies of GLEW")

if(GLEW_ENABLE_MX)
	# GLEW With Context Extensions.
	add_library(GLEWmx STATIC src/glew.c)
	set_target_properties(GLEWmx PROPERTIES DEFINE_SYMBOL "GLEW_STATIC;GLEW_MX")
	target_link_libraries(GLEWmx ${OPENGL_LIBRARIES})
	if(GLEW_INSTALL)
		install(TARGETS GLEWmx ARCHIVE DESTINATION .)
	endif()
endif()

# Examples
if(GLEW_BUILD_EXAMPLES)
	# GLEW Information Tool.
	add_executable(glewinfo src/glewinfo.c)
	add_dependencies(glewinfo GLEW)
	set_target_properties(glewinfo
		PROPERTIES DEFINE_SYMBOL "GLEW_STATIC")
	target_link_libraries(glewinfo GLEW ${OPENGL_LIBRARIES})

	# Visual Information tool
	add_executable(visualinfo src/visualinfo.c)
	add_dependencies(visualinfo GLEW)
	set_target_properties(visualinfo
		PROPERTIES DEFINE_SYMBOL "GLEW_STATIC")
	target_link_libraries(visualinfo GLEW ${OPENGL_LIBRARIES})
	if(GLEW_INSTALL)
		install(TARGETS glewinfo visualinfo RUNTIME DESTINATION .)
	endif()
endif()
