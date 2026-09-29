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
set_target_properties(qindaqt_keyring_storage PROPERTIES CXX_EXTENSIONS OFF)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_keyring_storage)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
add_library(qindaqt_keyring_service STATIC
    daemon/wire_types.cpp daemon/session_crypto.cpp daemon/private_directory.cpp
    daemon/collection_repository.cpp daemon/secret_service.cpp daemon/service_methods.cpp
    daemon/control_server.cpp daemon/item_methods.cpp daemon/properties.cpp daemon/prompts.cpp daemon/process_prompt_provider.cpp)
target_link_libraries(qindaqt_keyring_service PUBLIC QindaQt::KeyringStorage Qt6::Core Qt6::DBus PRIVATE OpenSSL::Crypto)
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
