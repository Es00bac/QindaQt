# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_android_native_bootstrap_tests tst_native_bootstrap.cpp)
target_link_libraries(qindaqt_android_native_bootstrap_tests
    PRIVATE QindaQt::AndroidNativeBootstrap QindaQt::AndroidManager Qt6::Test)
target_include_directories(qindaqt_android_native_bootstrap_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../../src/services/android_runtime/src")
target_compile_definitions(qindaqt_android_native_bootstrap_tests PRIVATE _GNU_SOURCE
    ANDROID_COLLECTOR_EXE="$<TARGET_FILE:qindaqt-android-collector>"
    ANDROID_COLLECTOR_RECIPE="${CMAKE_CURRENT_LIST_DIR}/../../../../src/services/android_runtime/data/collector.json")
add_dependencies(qindaqt_android_native_bootstrap_tests qindaqt-android-collector)
qindaqt_enable_warnings(qindaqt_android_native_bootstrap_tests)
add_test(NAME android-runtime.native-bootstrap COMMAND qindaqt_android_native_bootstrap_tests)
set_tests_properties(android-runtime.native-bootstrap PROPERTIES TIMEOUT 10
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DISPLAY=;WAYLAND_DISPLAY=")
