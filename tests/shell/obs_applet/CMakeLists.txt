# SPDX-License-Identifier: GPL-3.0-or-later

# One snapshot becomes one top bar: pure projection, no client and no QML.
qt_add_executable(qindaqt_obs_applet_presentation_tests
    tst_obs_applet_presentation.cpp)
target_link_libraries(qindaqt_obs_applet_presentation_tests
    PRIVATE qindaqt_shell_obs_applet Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_applet_presentation_tests)
add_test(NAME qindaqt.shell-obs-applet-presentation
    COMMAND qindaqt_obs_applet_presentation_tests)
set_tests_properties(qindaqt.shell-obs-applet-presentation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "shell;obs;model")

# The controller over a fake transport: the dispatch rules and the promise
# that an enabled control is a dispatchable one.
qt_add_executable(qindaqt_obs_applet_controller_tests
    tst_obs_applet_controller.cpp)
target_link_libraries(qindaqt_obs_applet_controller_tests
    PRIVATE qindaqt_shell_obs_applet_runtime Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_applet_controller_tests)
add_test(NAME qindaqt.shell-obs-applet-controller
    COMMAND qindaqt_obs_applet_controller_tests)
set_tests_properties(qindaqt.shell-obs-applet-controller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "shell;obs;model")

# The shell-private connection gate over fake OBS and confirmed preferences.
qt_add_executable(qindaqt_obs_applet_connection_tests
    tst_obs_applet_connection.cpp
    ../../../src/shell/runtime/obsappletconnection.cpp)
target_include_directories(qindaqt_obs_applet_connection_tests PRIVATE
    ../../../src/shell/runtime)
target_link_libraries(qindaqt_obs_applet_connection_tests PRIVATE
    QindaQt::ObsClient QindaQt::StreamingPreferences Qt6::Test)
qindaqt_enable_warnings(qindaqt_obs_applet_connection_tests)
add_test(NAME qindaqt.shell-obs-applet-connection
    COMMAND qindaqt_obs_applet_connection_tests)
set_tests_properties(qindaqt.shell-obs-applet-connection PROPERTIES
    LABELS "shell;obs;streaming")
