# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_library(qindaqt_global_menu_qt_widgets_adapter STATIC)
add_library(
    QindaQt::GlobalMenuQtWidgetsAdapter ALIAS qindaqt_global_menu_qt_widgets_adapter
)

target_sources(
    qindaqt_global_menu_qt_widgets_adapter
    PRIVATE
        src/qmenubar_menu_source.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/qt_widgets_adapter/qmenubar_menu_source.h
)
target_link_libraries(
    qindaqt_global_menu_qt_widgets_adapter
    PUBLIC
        QindaQt::GlobalMenuExporter
        QindaQt::GlobalMenuProtocol
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
target_compile_features(qindaqt_global_menu_qt_widgets_adapter PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_global_menu_qt_widgets_adapter PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_global_menu_qt_widgets_adapter)

install(
    TARGETS qindaqt_global_menu_qt_widgets_adapter
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
