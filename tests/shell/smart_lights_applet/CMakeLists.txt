# SPDX-License-Identifier: GPL-3.0-or-later

set(smart_lights_applet_source_root "${CMAKE_SOURCE_DIR}")

qt_add_executable(qindaqt_smart_lights_applet_presentation_tests
                  tst_smart_lights_applet_presentation.cpp)
target_link_libraries(
    qindaqt_smart_lights_applet_presentation_tests
    PRIVATE QindaQt::ShellSmartLightsApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_smart_lights_applet_presentation_tests)
add_test(
    NAME qindaqt.smart-lights-applet-presentation
    COMMAND qindaqt_smart_lights_applet_presentation_tests
)
set_tests_properties(
    qindaqt.smart-lights-applet-presentation
    PROPERTIES LABELS "shell;smart-lights;pure"
)

qt_add_executable(qindaqt_smart_lights_applet_request_tests
                  tst_smart_lights_request_state.cpp)
target_link_libraries(
    qindaqt_smart_lights_applet_request_tests
    PRIVATE QindaQt::ShellSmartLightsApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_smart_lights_applet_request_tests)
add_test(
    NAME qindaqt.smart-lights-applet-request-state
    COMMAND qindaqt_smart_lights_applet_request_tests
)
set_tests_properties(
    qindaqt.smart-lights-applet-request-state
    PROPERTIES LABELS "shell;smart-lights;pure"
)

# The fake transport header is listed so AUTOMOC generates its meta-object;
# it lives with the client tests that own it.
qt_add_executable(
    qindaqt_smart_lights_applet_controller_tests
    tst_smart_lights_applet_controller.cpp
    "${smart_lights_applet_source_root}/tests/services/wiz_client/support/fakewiztransport.h"
)
target_include_directories(
    qindaqt_smart_lights_applet_controller_tests
    PRIVATE "${smart_lights_applet_source_root}/tests/services/wiz_client"
)
target_link_libraries(
    qindaqt_smart_lights_applet_controller_tests
    PRIVATE QindaQt::ShellSmartLightsAppletRuntime Qt6::Test
)
qindaqt_enable_warnings(qindaqt_smart_lights_applet_controller_tests)
add_test(
    NAME qindaqt.smart-lights-applet-controller
    COMMAND qindaqt_smart_lights_applet_controller_tests
)
set_tests_properties(
    qindaqt.smart-lights-applet-controller
    PROPERTIES LABELS "shell;smart-lights;runtime"
)

qt_add_executable(
    qindaqt_smart_lights_applet_qml_tests
    tst_smart_lights_applet_qml.cpp
    "${smart_lights_applet_source_root}/tests/services/wiz_client/support/fakewiztransport.h"
)
target_include_directories(
    qindaqt_smart_lights_applet_qml_tests
    PRIVATE "${smart_lights_applet_source_root}/tests/services/wiz_client"
)
target_link_libraries(
    qindaqt_smart_lights_applet_qml_tests
    PRIVATE
        QindaQt::ShellSmartLightsAppletRuntime
        qindaqt_shell_smart_lights_applet_runtimeplugin
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_smart_lights_applet_qml_tests
    PRIVATE QINDAQT_SMART_LIGHTS_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
qindaqt_enable_warnings(qindaqt_smart_lights_applet_qml_tests)
qt_import_qml_plugins(qindaqt_smart_lights_applet_qml_tests)
add_test(
    NAME qindaqt.smart-lights-applet-offscreen
    COMMAND qindaqt_smart_lights_applet_qml_tests
)
set_tests_properties(
    qindaqt.smart-lights-applet-offscreen
    PROPERTIES
        LABELS "shell;smart-lights;runtime;offscreen;accessibility"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

# AGENT-GUARD: these rows construct a QGuiApplication, so they must name a
# platform. Without this they default to xcb and abort before their first
# assertion on any machine with no display -- a headless runner, or a
# developer on a VT -- which reads as a real failure and is not one. Every
# one of them passes under offscreen. Keep this list in step with the rows
# above; a new GUI row added without it fails the same way.
set_tests_properties(
    qindaqt.smart-lights-applet-controller
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
