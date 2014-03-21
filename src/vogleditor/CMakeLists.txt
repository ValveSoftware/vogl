project(vogleditor)
cmake_minimum_required(VERSION 2.8.9)

include("${SRC_DIR}/build_options.cmake")

if (NOT DEFINED ENV{QTDIR})
set(ENV{QTDIR} /usr/local/Trolltech/Qt-4.8.5)
endif()
find_package(Qt4 REQUIRED)

find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/voglcore
    ${SRC_DIR}/voglcommon
    ${CMAKE_BINARY_DIR}/voglinc
    ${SRC_DIR}/extlib/loki/include/loki
    ${SRC_DIR}/libtelemetry
    ${CMAKE_CURRENT_BINARY_DIR}
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTGUI_INCLUDE_DIR}
)

#include(${QT_USE_FILE})

## uncomment these to enable additional Qt modules
# set(QT_USE_QTNETWORK true)
# set(QT_USE_QTOPENGL true)
# set(QT_USE_QTSQL true)
# set(QT_USE_QTXML true)
# set(QT_USE_QTSVG true)
# set(QT_USE_QTTEST true)
# set(QT_USE_QTDBUS true)
# set(QT_USE_QTSCRIPT true)
# set(QT_USE_QTWEBKIT true)
# set(QT_USE_QTXMLPATTERNS true)
# set(QT_USE_PHONON true)

set(SRC_LIST
    main.cpp
    vogleditor.cpp
    vogleditor_apicalltreeitem.cpp
    vogleditor_apicalltimelinemodel.cpp
    vogleditor_gl_state_snapshot.cpp
    vogleditor_qapicalltreemodel.cpp
    vogleditor_qframebufferexplorer.cpp
    vogleditor_qprogramexplorer.cpp
    vogleditor_qshaderexplorer.cpp
    vogleditor_qstatetreemodel.cpp
    vogleditor_qtextureexplorer.cpp
    vogleditor_qtextureviewer.cpp
    vogleditor_qtimelineview.cpp
    vogleditor_qtrimdialog.cpp
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
    vogleditor_qframebufferexplorer.h
    vogleditor_qprogramexplorer.h
    vogleditor_qshaderexplorer.h
    vogleditor_qstatetreemodel.h
    vogleditor_qtextureviewer.h
    vogleditor_qtextureexplorer.h
    vogleditor_qtimelineview.h
    vogleditor_qtrimdialog.h
   )

# these are for non-QOBJECT headers
set(HEADER_LIST
    vogleditor.h
    vogleditor_apicallitem.h
    vogleditor_apicalltimelinemodel.h
    vogleditor_apicalltreeitem.h
    vogleditor_frameitem.h
    vogleditor_gl_state_snapshot.h
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
    vogleditor_qframebufferexplorer.ui
    vogleditor_qprogramexplorer.ui
    vogleditor_qshaderexplorer.ui
    vogleditor_qtextureexplorer.ui
    vogleditor_qtrimdialog.ui
   )

set(RESOURCE_LIST
   )

QT4_WRAP_CPP(QT_GEN_HEADER_MOC_LIST ${UI_HEADER_LIST})
QT4_WRAP_UI(QT_GEN_FORM_HEADER_LIST ${FORM_LIST})
QT4_ADD_RESOURCES(QT_GEN_RESOURCE_RCC_LIST ${RESOURCE_LIST})

#add_definitions(${QT_DEFINITIONS})

add_executable(${PROJECT_NAME} ${SRC_LIST} ${HEADER_LIST}
        ${QT_GEN_HEADER_MOC_LIST}
        ${QT_GEN_FORM_HEADER_LIST}
        ${QT_GEN_RESOURCE_RCC_LIST}
   )

add_dependencies(${PROJECT_NAME} voglgen_make_inc)

target_link_libraries(${PROJECT_NAME}
	${QT_QTMAIN_LIBRARY} 
	${QT_QTCORE_LIBRARY} 
	${QT_QTGUI_LIBRARY} 
	${TELEMETRY_LIBRARY}
	${X11_X11_LIB}
	backtrace
	voglcommon
    voglcore
    ${CMAKE_DL_LIBS}
   )

build_options_finalize()
