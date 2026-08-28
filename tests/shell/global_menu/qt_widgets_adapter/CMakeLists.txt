# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_global_menu_qt_widgets_adapter_tests tst_qmenubar_menu_source.cpp)
target_link_libraries(
    qindaqt_global_menu_qt_widgets_adapter_tests
    PRIVATE
        QindaQt::GlobalMenuQtWidgetsAdapter
        QindaQt::GlobalMenuExporter
        QindaQt::GlobalMenuProtocol
        Qt6::Widgets
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_qt_widgets_adapter_tests)
add_test(
    NAME qindaqt.global-menu-qt-widgets-adapter
    COMMAND qindaqt_global_menu_qt_widgets_adapter_tests
)
set_tests_properties(
    qindaqt.global-menu-qt-widgets-adapter
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
