# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Qml Quick QuickControls2)

qt_add_library(qindaqt_global_menu_applet STATIC)
add_library(QindaQt::GlobalMenuApplet ALIAS qindaqt_global_menu_applet)

target_sources(
    qindaqt_global_menu_applet
    PRIVATE
        src/globalmenuappletaccess.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/applet/globalmenuappletaccess.h
)
target_include_directories(
    qindaqt_global_menu_applet
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(qindaqt_global_menu_applet PUBLIC QindaQt::GlobalMenuProtocol Qt6::Core)
target_compile_features(qindaqt_global_menu_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_global_menu_applet PROPERTIES CXX_EXTENSIONS OFF AUTOMOC_PATH_PREFIX ON)
qindaqt_enable_warnings(qindaqt_global_menu_applet)

install(
    TARGETS qindaqt_global_menu_applet
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

set(QINDAQT_GLOBAL_MENU_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/GlobalMenu")
qt_add_library(qindaqt_global_menu_qml SHARED)
add_library(QindaQt::GlobalMenuQml ALIAS qindaqt_global_menu_qml)
qt_add_qml_module(
    qindaqt_global_menu_qml
    URI QindaQt.Shell.GlobalMenu
    VERSION 1.0
    OUTPUT_DIRECTORY "${QINDAQT_GLOBAL_MENU_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_global_menu.qmltypes
    QML_FILES
        qml/GlobalMenuActionEntry.qml
        qml/GlobalMenuApplet.qml
        qml/GlobalMenuNativeMenu.qml
        qml/GlobalMenuNativeMenuItem.qml
)
target_link_libraries(
    qindaqt_global_menu_qml
    PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(qindaqt_global_menu_qml PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_global_menu_qml)

# The narrow install exposes a complete compiled QML module. The production
# shell also stages the backing library in its private loader directory via
# src/shell/CMakeLists.txt because it links this audited renderer directly.
install(
    TARGETS qindaqt_global_menu_qml qindaqt_global_menu_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/GlobalMenu"
        COMPONENT GlobalMenuAppletRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/GlobalMenu"
        COMPONENT GlobalMenuAppletRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/GlobalMenu"
        COMPONENT GlobalMenuAppletRuntime
)
install(
    FILES
        "${QINDAQT_GLOBAL_MENU_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_GLOBAL_MENU_QML_OUTPUT_DIRECTORY}/qindaqt_global_menu.qmltypes"
        qml/GlobalMenuActionEntry.qml
        qml/GlobalMenuApplet.qml
        qml/GlobalMenuNativeMenu.qml
        qml/GlobalMenuNativeMenuItem.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/GlobalMenu"
    COMPONENT GlobalMenuAppletRuntime
)
