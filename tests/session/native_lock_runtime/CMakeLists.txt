# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_native_lock_runtime_tests
    tst_native_lock_runtime.cpp)
target_link_libraries(qindaqt_native_lock_runtime_tests PRIVATE
    qindaqt_native_lock_runtime qindaqt_settings_screensaver QindaQt::SessionLockState
    QindaQt::LockPreferences QindaQt::SettingsClient QindaQt::SettingsProtocol
    QindaQt::PowerClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_native_lock_runtime_tests)
add_test(NAME qindaqt.session-native-lock-runtime
    COMMAND qindaqt_native_lock_runtime_tests)
set_tests_properties(qindaqt.session-native-lock-runtime PROPERTIES
    LABELS "session;lock;power;settings")
