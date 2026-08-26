# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_shell_visibility_snapshot_tests tst_shellvisibilitysnapshot.cpp)
target_link_libraries(
    qindaqt_shell_visibility_snapshot_tests
    PRIVATE QindaQt::CompositorBridge Qt6::Test
)
add_test(
    NAME compositor.shell-visibility-snapshot
    COMMAND qindaqt_shell_visibility_snapshot_tests
)

qt_add_executable(
    qindaqt_shell_visibility_window_admission_tests
    tst_shellvisibilitywindowadmission.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/shellvisibilitywindowadmission.cpp"
)
target_include_directories(
    qindaqt_shell_visibility_window_admission_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_shell_visibility_window_admission_tests
    PRIVATE Qt6::Core Qt6::Test
)
add_test(
    NAME compositor.shell-visibility-window-admission
    COMMAND qindaqt_shell_visibility_window_admission_tests
)

qt_add_executable(
    qindaqt_shell_visibility_refresh_scheduler_tests
    tst_shellvisibilityrefreshscheduler.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/shellvisibilityrefreshscheduler.cpp"
)
target_include_directories(
    qindaqt_shell_visibility_refresh_scheduler_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_shell_visibility_refresh_scheduler_tests
    PRIVATE Qt6::Core Qt6::Test
)
add_test(
    NAME compositor.shell-visibility-refresh-scheduler
    COMMAND qindaqt_shell_visibility_refresh_scheduler_tests
)
