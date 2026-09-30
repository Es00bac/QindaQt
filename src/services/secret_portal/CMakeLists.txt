# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)
qt_add_library(qindaqt_secret_portal_policy STATIC)
add_library(QindaQt::SecretPortalPolicy ALIAS qindaqt_secret_portal_policy)
target_sources(qindaqt_secret_portal_policy PRIVATE src/secret_policy.cpp src/legacy_import.cpp src/legacy_import_store.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES include/qindaqt/services/secret_portal/secret_policy.h include/qindaqt/services/secret_portal/legacy_import.h)
target_link_libraries(qindaqt_secret_portal_policy PUBLIC QindaQt::KeyringStorage Qt6::Core PRIVATE OpenSSL::Crypto)
target_compile_features(qindaqt_secret_portal_policy PUBLIC cxx_std_20)
set_target_properties(qindaqt_secret_portal_policy PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_secret_portal_policy)
qt_add_library(qindaqt_secret_portal STATIC)
add_library(QindaQt::SecretPortal ALIAS qindaqt_secret_portal)
target_sources(qindaqt_secret_portal PRIVATE src/qt_keyring_broker.cpp src/fd_writer.cpp src/fd_writer_p.h src/secret_portal_adaptor.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES include/qindaqt/services/secret_portal/secret_broker.h include/qindaqt/services/secret_portal/secret_portal_adaptor.h)
target_link_libraries(qindaqt_secret_portal PUBLIC QindaQt::SecretPortalPolicy Qt6::Core Qt6::DBus PRIVATE QindaQt::KeyringProtocol)
target_compile_features(qindaqt_secret_portal PUBLIC cxx_std_20)
set_target_properties(qindaqt_secret_portal PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_secret_portal)
