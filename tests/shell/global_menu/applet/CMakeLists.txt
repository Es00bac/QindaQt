# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_global_menu_applet_access_tests tst_globalmenuappletaccess.cpp)
target_link_libraries(
    qindaqt_global_menu_applet_access_tests
    PRIVATE QindaQt::GlobalMenuApplet QindaQt::GlobalMenuProtocol Qt6::Test
)
qindaqt_enable_warnings(qindaqt_global_menu_applet_access_tests)
add_test(
    NAME qindaqt.global-menu-applet-access
    COMMAND qindaqt_global_menu_applet_access_tests
)

qt_add_executable(
    qindaqt_global_menu_native_popup_placement_tests
    tst_nativepopupplacement.cpp
    "${PROJECT_SOURCE_DIR}/src/shell/global_menu/applet/src/nativepopupplacement.cpp"
)
target_include_directories(
    qindaqt_global_menu_native_popup_placement_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/shell/global_menu/applet/src"
)
target_link_libraries(
    qindaqt_global_menu_native_popup_placement_tests
    PRIVATE Qt6::Test Qt6::Gui Qt6::Qml
)
qindaqt_enable_warnings(qindaqt_global_menu_native_popup_placement_tests)
add_test(
    NAME qindaqt.global-menu-native-popup-placement
    COMMAND qindaqt_global_menu_native_popup_placement_tests
)
set_tests_properties(
    qindaqt.global-menu-native-popup-placement
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
