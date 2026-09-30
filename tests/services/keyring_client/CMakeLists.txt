# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(tst_keyring_client tst_keyring_client.cpp)
target_link_libraries(tst_keyring_client PRIVATE QindaQt::KeyringClient QindaQt::KeyringProtocol Qt6::Test)
target_include_directories(tst_keyring_client PRIVATE ${PROJECT_SOURCE_DIR}/src/services/keyring_client)
target_compile_definitions(tst_keyring_client PRIVATE
 QINDAQT_KEYRING_BINARY="$<TARGET_FILE:qindaqt-keyring>"
 QINDAQT_PROMPT_SCRIPT="${PROJECT_SOURCE_DIR}/tests/services/keyring/native_scripted_prompt.py")
add_test(NAME keyring_client COMMAND tst_keyring_client)
set_tests_properties(keyring_client PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 90)
qt_add_executable(tst_keyring_settings_model tst_keyring_settings_model.cpp)
target_link_libraries(tst_keyring_settings_model PRIVATE QindaQt::SettingsKeyring Qt6::Test)
add_test(NAME keyring_settings_model COMMAND tst_keyring_settings_model)
set_tests_properties(keyring_settings_model PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 30)

qt_add_executable(tst_keyring_preferences tst_keyring_preferences.cpp)
target_link_libraries(tst_keyring_preferences PRIVATE QindaQt::SettingsKeyring QindaQt::Settings Qt6::Test)
target_compile_definitions(tst_keyring_preferences PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME keyring_preferences COMMAND tst_keyring_preferences)
set_tests_properties(keyring_preferences PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 30)
