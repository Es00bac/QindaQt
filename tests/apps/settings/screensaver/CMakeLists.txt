# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_screensaver_settings_model_tests
    tst_screensaver_settings_model.cpp
    screensaver_model_test_support.h)
target_link_libraries(qindaqt_screensaver_settings_model_tests
    PRIVATE qindaqt_settings_screensaver Qt6::Test)
qindaqt_enable_warnings(qindaqt_screensaver_settings_model_tests)
add_test(NAME qindaqt.settings-screensaver-model
    COMMAND qindaqt_screensaver_settings_model_tests)
set_tests_properties(qindaqt.settings-screensaver-model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "settings;screensaver;model")

# The greeter mirror's file format is KConfig's, so its row reads the file back
# with the same library the store writes it with.
find_package(KF6Config 6.0 REQUIRED CONFIG)

qt_add_executable(qindaqt_lock_screen_saver_store_tests
    tst_lock_screen_saver_store.cpp)
target_link_libraries(qindaqt_lock_screen_saver_store_tests
    PRIVATE qindaqt_settings_screensaver KF6::ConfigCore Qt6::Test)
qindaqt_enable_warnings(qindaqt_lock_screen_saver_store_tests)
add_test(NAME qindaqt.settings-screensaver-lock-screen-saver-store
    COMMAND qindaqt_lock_screen_saver_store_tests)
set_tests_properties(qindaqt.settings-screensaver-lock-screen-saver-store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "settings;screensaver;screen-lock")

qt_add_executable(qindaqt_screensaver_preview_tests
    tst_screensaver_preview.cpp)
target_link_libraries(qindaqt_screensaver_preview_tests
    PRIVATE qindaqt_settings_screensaver Qt6::Test)
qindaqt_enable_warnings(qindaqt_screensaver_preview_tests)
add_test(NAME qindaqt.settings-screensaver-preview
    COMMAND qindaqt_screensaver_preview_tests)
set_tests_properties(qindaqt.settings-screensaver-preview PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "settings;screensaver;preview")

qt_add_executable(qindaqt_screensaver_page_tests
    tst_screensaver_page.cpp)
target_include_directories(qindaqt_screensaver_page_tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(qindaqt_screensaver_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_SCREENSAVER_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/screensaver/qml/ScreensaverPage.qml")
target_link_libraries(qindaqt_screensaver_page_tests PRIVATE
    qindaqt_settings_appearance QindaQt::TokensQml
    qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_dependencies(qindaqt_screensaver_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin)
qindaqt_enable_warnings(qindaqt_screensaver_page_tests)
add_test(NAME qindaqt.settings-screensaver-page COMMAND qindaqt_screensaver_page_tests)
set_tests_properties(qindaqt.settings-screensaver-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;screensaver;accessibility")
