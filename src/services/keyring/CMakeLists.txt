# SPDX-License-Identifier: GPL-3.0-or-later
# OpenSSL 3.2 introduced Argon2; runtime provider availability fails closed.
find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)
add_library(qindaqt_keyring_storage STATIC
    src/secure_buffer.cpp src/crypto.cpp src/format.cpp
    src/atomic_file.cpp src/collection_store.cpp)
add_library(QindaQt::KeyringStorage ALIAS qindaqt_keyring_storage)
target_include_directories(qindaqt_keyring_storage PUBLIC include)
target_link_libraries(qindaqt_keyring_storage PRIVATE OpenSSL::Crypto)
target_compile_features(qindaqt_keyring_storage PUBLIC cxx_std_20)
set_target_properties(qindaqt_keyring_storage PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_keyring_storage)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
# Owning one-time catalog adapter: consumers never read/write the private schema.
add_library(qindaqt_keyring_import_store STATIC daemon/private_directory.cpp
    daemon/collection_repository.cpp daemon/collection_import_transaction.cpp
    import/collection_import_catalog.cpp)
add_library(QindaQt::KeyringImportStore ALIAS qindaqt_keyring_import_store)
target_link_libraries(qindaqt_keyring_import_store PUBLIC QindaQt::KeyringStorage Qt6::Core
    PRIVATE QindaQt::KeyringProtocol Qt6::DBus OpenSSL::Crypto)
qindaqt_enable_warnings(qindaqt_keyring_import_store)

add_library(qindaqt_keyring_service STATIC
    daemon/session_crypto.cpp daemon/secret_service.cpp daemon/service_methods.cpp
    daemon/control_server.cpp daemon/item_methods.cpp daemon/properties.cpp daemon/prompts.cpp daemon/process_prompt_provider.cpp daemon/session_display_binding.cpp daemon/native_item_methods.cpp daemon/prompt_password_change.cpp daemon/portal_methods.cpp daemon/metadata_receipts.cpp)
target_link_libraries(qindaqt_keyring_service PUBLIC QindaQt::KeyringStorage QindaQt::KeyringProtocol Qt6::Core Qt6::DBus PRIVATE QindaQt::KeyringImportStore OpenSSL::Crypto QindaQt::CompositorNames QindaQt::SecretPortalPolicy)
target_include_directories(qindaqt_keyring_service PRIVATE daemon)
qt_add_resources(qindaqt_keyring_service keyring_api PREFIX "/keyring" BASE data FILES data/api.xml)
qt_add_executable(qindaqt-keyring app/main.cpp)
target_link_libraries(qindaqt-keyring PRIVATE qindaqt_keyring_service)
target_compile_definitions(qindaqt-keyring PRIVATE QINDAQT_KEYRING_PROMPT_PATH="${CMAKE_INSTALL_FULL_BINDIR}/qindaqt-keyring-prompt")
qindaqt_enable_warnings(qindaqt_keyring_service)
qindaqt_enable_warnings(qindaqt-keyring)

if(TARGET QindaQt::AuthenticationOverlay)
    add_subdirectory(prompt)
endif()

install(TARGETS qindaqt-keyring RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT QindaQtKeyring)
foreach(unit qindaqt-keyring.service org.freedesktop.secrets.service org.qindaqt.Keyring1.service)
    configure_file("data/${unit}.in" "${CMAKE_CURRENT_BINARY_DIR}/${unit}" @ONLY)
endforeach()
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-keyring.service" data/qindaqt-keyring.socket
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/user" COMPONENT QindaQtKeyring)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.secrets.service"
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Keyring1.service"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services" COMPONENT QindaQtKeyring)

add_subdirectory(pam)

# Shared ordinary identity/idle boundaries; this daemon retains session selection,
# legacy activation compatibility and collection lock/disclosure policy.
target_link_libraries(qindaqt_keyring_service PUBLIC QindaQt::CompositorAttachment QindaQt::IdleObservation)

target_sources(qindaqt_keyring_service PRIVATE daemon/lock_policy.h daemon/lock_policy.cpp)
target_link_libraries(qindaqt_keyring_service PRIVATE QindaQt::SettingsClient QindaQt::SessionLockState)

target_sources(qindaqt_keyring_service PRIVATE daemon/resident_lock_policy.h daemon/resident_lock_policy.cpp)
