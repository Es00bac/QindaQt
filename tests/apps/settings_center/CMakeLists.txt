# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS QuickTest)
add_test(
    NAME qindaqt.settings-app-offscreen
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_settingsapp.qml"
)
set_tests_properties(
    qindaqt.settings-app-offscreen PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)
add_test(
    NAME qindaqt.settings-app-rejects-unknown-route
    COMMAND "${CMAKE_COMMAND}"
            -DSETTINGS_EXECUTABLE=$<TARGET_FILE:qindaqt-settings>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_invalid_route.cmake"
)
set_tests_properties(
    qindaqt.settings-app-rejects-unknown-route PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)
add_test(
    NAME qindaqt.settings-app-desktop-identity
    COMMAND "${CMAKE_COMMAND}"
            -DSETTINGS_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/apps/settings_center
            -DSETTINGS_EXECUTABLE=$<TARGET_FILE:qindaqt-settings>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_desktop_identity.cmake"
)
