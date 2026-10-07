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
