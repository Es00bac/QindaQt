# SPDX-License-Identifier: GPL-3.0-or-later

# Focused rows for the Display route's arrangement canvas and scale guidance.
# They drive the real DisplaySettingsModel over the fake transport so every
# drafted position passes production topology admission.
function(qindaqt_add_display_arrangement_test target source row)
    qt_add_executable(${target}
                      ${source}
                      arrangement_test_support.h
                      ${PROJECT_SOURCE_DIR}/tests/services/display_client/support/fake_display_transport.h)
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR})
    target_compile_definitions(
        ${target} PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_DISPLAY_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/display/qml/DisplayPage.qml"
    )
    target_link_libraries(
        ${target}
        PRIVATE qindaqt_settings_display qindaqt_settings_appearance QindaQt::TokensQml
                Qt6::Quick Qt6::QuickControls2 Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_dependencies(
        ${target}
        qindaqt_tokens_qmlplugin
        qindaqt_controls_qmlplugin
        qindaqt_settings_display_qmlplugin
        qindaqt_settings_appearance_qmlplugin
    )
    add_test(NAME ${row} COMMAND ${target})
    set_tests_properties(
        ${row} PROPERTIES
        LABELS "offscreen;accessibility"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
    )
endfunction()

qindaqt_add_display_arrangement_test(
    qindaqt_display_arrangement_canvas_tests
    tst_display_arrangement_canvas.cpp
    qindaqt.display-arrangement-canvas)

qindaqt_add_display_arrangement_test(
    qindaqt_display_arrangement_scale_tests
    tst_display_arrangement_scale.cpp
    qindaqt.display-arrangement-scale)
