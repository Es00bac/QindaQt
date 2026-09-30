# SPDX-License-Identifier: GPL-3.0-or-later
add_library(qindaqt_keyring_client STATIC qt_keyring_gateway.cpp keyring_reply_validation.cpp
    include/qindaqt/services/keyring_client/keyring_gateway.h
    include/qindaqt/services/keyring_client/qt_keyring_gateway.h)
add_library(QindaQt::KeyringClient ALIAS qindaqt_keyring_client)
target_include_directories(qindaqt_keyring_client PUBLIC include)
target_link_libraries(qindaqt_keyring_client PUBLIC Qt6::Core Qt6::DBus QindaQt::KeyringStorage PRIVATE QindaQt::KeyringProtocol)
target_compile_features(qindaqt_keyring_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_keyring_client PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_keyring_client)
