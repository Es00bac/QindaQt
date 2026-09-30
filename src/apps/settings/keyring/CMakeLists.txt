# SPDX-License-Identifier: GPL-3.0-or-later
add_library(qindaqt_settings_keyring STATIC keyring_settings_model.cpp keyring_preferences.cpp
    include/qindaqt/apps/settings_keyring/keyring_preferences.h
    include/qindaqt/apps/settings_keyring/keyring_settings_model.h)
add_library(QindaQt::SettingsKeyring ALIAS qindaqt_settings_keyring)
target_include_directories(qindaqt_settings_keyring PUBLIC include)
target_link_libraries(qindaqt_settings_keyring PUBLIC QindaQt::KeyringClient QindaQt::SettingsClient Qt6::Core)
target_compile_features(qindaqt_settings_keyring PUBLIC cxx_std_20)
set_target_properties(qindaqt_settings_keyring PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_settings_keyring)

# GUI composition owns selection; pure SettingsKeyring remains QtCore-only.
add_library(qindaqt_keyring_clipboard STATIC sensitive_clipboard.cpp
 include/qindaqt/apps/settings_keyring/sensitive_clipboard.h)
add_library(QindaQt::KeyringClipboard ALIAS qindaqt_keyring_clipboard)
target_include_directories(qindaqt_keyring_clipboard PUBLIC include)
target_link_libraries(qindaqt_keyring_clipboard PUBLIC Qt6::Core Qt6::Gui QindaQt::KeyringStorage)
target_compile_features(qindaqt_keyring_clipboard PUBLIC cxx_std_20)
set_target_properties(qindaqt_keyring_clipboard PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_keyring_clipboard)
