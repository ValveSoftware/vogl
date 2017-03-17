project(vogleditor)
cmake_minimum_required(VERSION 2.8.9)

include("${SRC_DIR}/build_options.cmake")

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${SRC_DIR}/cmake/Modules/")

# we want cmake to link the Qt libs into the binary
# This policy was introduced in 2.8.11, so on newer versions, use the OLD policy to maintain consistent behavior
if (POLICY CMP0020)
   cmake_policy(SET CMP0020 OLD)
endif()

find_package(Qt5 COMPONENTS Widgets Gui Core REQUIRED)
find_package(Threads REQUIRED)

require_pthreads()
request_backtrace()
require_sdl2()
require_app()

include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/voglcommon
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/extlib/loki/include/loki
    ${SRC_DIR}/extlib/pxfmt
    ${SRC_DIR}/libtelemetry
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Qt5Widgets_INCLUDE_DIRS}
    ${SDL2_INCLUDE}
    ${LibBackTrace_INCLUDE}
)

set(SRC_LIST
    main.cpp
    vogleditor.cpp
    vogleditor_apicalltreeitem.cpp
    vogleditor_apicalltimelinemodel.cpp
    vogleditor_gl_state_snapshot.cpp
    vogleditor_qapicalltreemodel.cpp
    vogleditor_qbufferexplorer.cpp
    vogleditor_qframebufferexplorer.cpp
    vogleditor_qlaunchtracerdialog.cpp
    vogleditor_qprogramarbexplorer.cpp
    vogleditor_qprogramexplorer.cpp
    vogleditor_qsettings.cpp
    vogleditor_qsettingsdialog.cpp
    vogleditor_qshaderexplorer.cpp
    vogleditor_qsnapshotoverlaywidget.cpp
    vogleditor_qstatetreemodel.cpp
    vogleditor_qtextureexplorer.cpp
    vogleditor_qtextureviewer.cpp
    vogleditor_qtimelineview.cpp
    vogleditor_qtrimdialog.cpp
    vogleditor_qdumpdialog.cpp
    vogleditor_qvertexarrayexplorer.cpp
    vogleditor_qvertexvisualizer.cpp
    vogleditor_output.cpp
    vogleditor_statetreearbprogramitem.cpp
    vogleditor_statetreearbprogramenvitem.cpp
    vogleditor_statetreebufferitem.cpp
    vogleditor_statetreecontextgeneralitem.cpp
    vogleditor_statetreecontextinfoitem.cpp
    vogleditor_statetreecontextitem.cpp
    vogleditor_statetreedisplaylistitem.cpp
    vogleditor_statetreeframebufferitem.cpp
    vogleditor_statetreeitem.cpp
    vogleditor_statetreelightitem.cpp
    vogleditor_statetreematrixitem.cpp
    vogleditor_statetreepolygonstippleitem.cpp
    vogleditor_statetreeprogramitem.cpp
    vogleditor_statetreeprogrampipelineitem.cpp
    vogleditor_statetreequeryitem.cpp
    vogleditor_statetreerenderbufferitem.cpp
    vogleditor_statetreesampleritem.cpp
    vogleditor_statetreeshaderitem.cpp
    vogleditor_statetreesyncitem.cpp
    vogleditor_statetreetexenvitem.cpp
    vogleditor_statetreetextureitem.cpp
    vogleditor_statetreevertexarrayitem.cpp
    vogleditor_timelineitem.cpp
    vogleditor_timelinemodel.cpp
    vogleditor_tracereplayer.cpp
   )

# This should only contain headers that define a QOBJECT
# Typically that means just headers for UI objects
set(UI_HEADER_LIST
    vogleditor.h
    vogleditor_qapicalltreemodel.h
    vogleditor_qbufferexplorer.h
    vogleditor_qframebufferexplorer.h
    vogleditor_qlaunchtracerdialog.h
    vogleditor_qprogramarbexplorer.h
    vogleditor_qprogramexplorer.h
    vogleditor_qsettings.h
    vogleditor_qsettingsdialog.h
    vogleditor_qshaderexplorer.h
    vogleditor_qsnapshotoverlaywidget.h
    vogleditor_qstatetreemodel.h
    vogleditor_qtextureviewer.h
    vogleditor_qtextureexplorer.h
    vogleditor_qtimelineview.h
    vogleditor_qtrimdialog.h
    vogleditor_qdumpdialog.h
    vogleditor_qvertexarrayexplorer.h
    vogleditor_qvertexvisualizer.h
   )

# these are for non-QOBJECT headers
set(HEADER_LIST
    vogleditor.h
    vogleditor_apicallitem.h
    vogleditor_apicalltimelinemodel.h
    vogleditor_apicalltreeitem.h
    vogleditor_frameitem.h
    vogleditor_gl_state_snapshot.h
    vogleditor_output.h
    vogleditor_snapshotitem.h
    vogleditor_statetreearbprogramitem.h
    vogleditor_statetreearbprogramenvitem.h
    vogleditor_statetreebufferitem.h
    vogleditor_statetreecontextgeneralitem.h
    vogleditor_statetreecontextinfoitem.h
    vogleditor_statetreecontextitem.h
    vogleditor_statetreedisplaylistitem.h
    vogleditor_statetreeitem.h
    vogleditor_statetreelightitem.h
    vogleditor_statetreematrixitem.h
    vogleditor_statetreepolygonstippleitem.h
    vogleditor_statetreeprogramitem.h
    vogleditor_statetreequeryitem.h
    vogleditor_statetreerenderbufferitem.h
    vogleditor_statetreesampleritem.h
    vogleditor_statetreeshaderitem.h
    vogleditor_statetreesyncitem.h
    vogleditor_statetreetexenvitem.h
    vogleditor_statetreetextureitem.h
    vogleditor_statetreevertexarrayitem.h
    vogleditor_statetreeframebufferitem.h
    vogleditor_timelineitem.h
    vogleditor_timelinemodel.h
    vogleditor_tracereplayer.h
   )

set(FORM_LIST
    vogleditor.ui
    vogleditor_qbufferexplorer.ui
    vogleditor_qframebufferexplorer.ui
    vogleditor_qlaunchtracerdialog.ui
    vogleditor_qprogramarbexplorer.ui
    vogleditor_qprogramexplorer.ui
    vogleditor_qsettingsdialog.ui
    vogleditor_qshaderexplorer.ui
    vogleditor_qtextureexplorer.ui
    vogleditor_qtrimdialog.ui
    vogleditor_qdumpdialog.ui
    vogleditor_qvertexarrayexplorer.ui
   )

set(RESOURCE_LIST
   )

QT5_WRAP_CPP(QT_GEN_HEADER_MOC_LIST ${UI_HEADER_LIST})
QT5_WRAP_UI(QT_GEN_FORM_HEADER_LIST ${FORM_LIST})
QT5_ADD_RESOURCES(QT_GEN_RESOURCE_RCC_LIST ${RESOURCE_LIST})

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    add_compiler_flag("-Wno-global-constructors -Wno-used-but-marked-unused")
endif()

# Platform specific compile flags.
if (NOT MSVC)
    add_compiler_flag("-fPIC")
endif()

add_executable(${PROJECT_NAME} ${SRC_LIST} ${HEADER_LIST}
    ${QT_GEN_HEADER_MOC_LIST}
    ${QT_GEN_FORM_HEADER_LIST}
    ${QT_GEN_RESOURCE_RCC_LIST}
   )

add_dependencies(${PROJECT_NAME} voglgen_make_inc)
if (TARGET SDL)
    add_dependencies(${PROJECT_NAME} SDL)
endif ()

target_link_libraries(${PROJECT_NAME}
    Qt5::Widgets
    Qt5::Core
    ${TELEMETRY_LIBRARY}
    ${LibBackTrace_LIBRARY}
    voglcommon
    voglcore
    pxfmt
    ${CMAKE_DL_LIBS}
    ${SDL2_LIBRARY}
    ${APP_LIBRARY}
)

if (MSVC)
  # copy the debug and release dlls for Qt5Widgets, Qt5Core, and Qt5Gui
  set (COPY_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR})

  get_property(qt5_qmake_executable TARGET ${Qt5Core_QMAKE_EXECUTABLE} PROPERTY IMPORT_LOCATION)
  execute_process(COMMAND ${qt5_qmake_executable} -query QT_INSTALL_PLUGINS OUTPUT_VARIABLE QT_INSTALL_PLUGINS OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND ${qt5_qmake_executable} -query QT_INSTALL_BINS OUTPUT_VARIABLE QT_INSTALL_BINS OUTPUT_STRIP_TRAILING_WHITESPACE)

  # There are also several other files that need to be copied or created
  FILE(WRITE ${CMAKE_CURRENT_LIST_DIR}/qt.conf "[Paths]\nPlugins=.")
  add_custom_target(copy_deps_qt5 ALL
                    ${CMAKE_COMMAND} -E make_directory "${COPY_DEST}/platforms"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/qt.conf" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/Qt5Widgets.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/Qt5Widgetsd.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/Qt5Core.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/Qt5Cored.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/Qt5Gui.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/Qt5Guid.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/icudt52.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/icuin52.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_BINS}/icuuc52.dll" "${COPY_DEST}"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_PLUGINS}/platforms/qwindows.dll" "${COPY_DEST}/platforms"
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_INSTALL_PLUGINS}/platforms/qwindowsd.dll"  "${COPY_DEST}/platforms"
					COMMENT "Copying vogleditor Qt5 dependencies to ${COPY_DEST}"
					VERBATIM)
endif()

build_options_finalize()

#
#  Installation
#
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
