# SPDX-License-Identifier: GPL-3.0-or-later
# Actual copier safety regressions use the existing private local boundary.
# No AppShell, KIO, host buses or GUI event loop is required.
qt_add_executable(qindaqt_file_manager_copy_safety_tests tst_copy_safety.cpp)
target_link_libraries(qindaqt_file_manager_copy_safety_tests
    PRIVATE qindaqt_file_manager_desktop_boundary Qt6::Test)
target_compile_features(qindaqt_file_manager_copy_safety_tests PRIVATE cxx_std_20)
qindaqt_enable_warnings(qindaqt_file_manager_copy_safety_tests)
add_test(NAME qindaqt.file-manager-copy-safety
    COMMAND qindaqt_file_manager_copy_safety_tests)
set_tests_properties(qindaqt.file-manager-copy-safety PROPERTIES
    LABELS "file-manager;mutation;copy-safety"
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")

qindaqt_add_file_manager_test(qindaqt_file_manager_mutation_output_tests
    tst_mutation_output.cpp qindaqt.file-manager-mutation-output)
set_tests_properties(qindaqt.file-manager-mutation-output PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")

add_test(NAME qindaqt.file-manager-mutation-output-qml
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_mutation_output.qml")
set_tests_properties(qindaqt.file-manager-mutation-output-qml PROPERTIES
    LABELS "file-manager;mutation;copy-safety"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;QT_QUICK_CONTROLS_STYLE=Fusion;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")
