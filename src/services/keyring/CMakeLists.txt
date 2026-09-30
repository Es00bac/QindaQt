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
add_library(qindaqt_keyring_service STATIC
    daemon/session_crypto.cpp daemon/private_directory.cpp
    daemon/collection_repository.cpp daemon/secret_service.cpp daemon/service_methods.cpp
    daemon/control_server.cpp daemon/item_methods.cpp daemon/properties.cpp daemon/prompts.cpp daemon/process_prompt_provider.cpp daemon/session_display_binding.cpp daemon/native_item_methods.cpp daemon/prompt_password_change.cpp)
target_link_libraries(qindaqt_keyring_service PUBLIC QindaQt::KeyringStorage QindaQt::KeyringProtocol Qt6::Core Qt6::DBus PRIVATE OpenSSL::Crypto QindaQt::CompositorNames)
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

# QCore-only true idle observer consumes a caller-pinned ordinary display FD.
find_package(Wayland REQUIRED COMPONENTS Client)
find_package(PkgConfig REQUIRED)
pkg_get_variable(QINDAQT_WAYLAND_PROTOCOL_DATA wayland-protocols pkgdatadir)
find_program(QINDAQT_WAYLAND_SCANNER wayland-scanner REQUIRED)
set(QINDAQT_IDLE_XML "${QINDAQT_WAYLAND_PROTOCOL_DATA}/staging/ext-idle-notify/ext-idle-notify-v1.xml")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-client.h" "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-protocol.c"
 COMMAND ${QINDAQT_WAYLAND_SCANNER} client-header ${QINDAQT_IDLE_XML} "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-client.h"
 COMMAND ${QINDAQT_WAYLAND_SCANNER} private-code ${QINDAQT_IDLE_XML} "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-protocol.c"
 DEPENDS ${QINDAQT_IDLE_XML} VERBATIM)
enable_language(C)
target_sources(qindaqt_keyring_service PRIVATE daemon/idle_observer.h daemon/idle_observer.cpp "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-protocol.c")
target_include_directories(qindaqt_keyring_service PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_keyring_service PRIVATE Wayland::Client)

target_sources(qindaqt_keyring_service PRIVATE daemon/lock_policy.h daemon/lock_policy.cpp)
target_link_libraries(qindaqt_keyring_service PRIVATE QindaQt::SettingsClient QindaQt::SessionLockState)

target_sources(qindaqt_keyring_service PRIVATE daemon/resident_lock_policy.h daemon/resident_lock_policy.cpp)
