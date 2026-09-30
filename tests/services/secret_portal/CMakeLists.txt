# SPDX-License-Identifier: GPL-3.0-or-later
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtSecretPortalTests LANGUAGES CXX)
    include(CTest)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
    find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)
    set(CMAKE_AUTOMOC ON)
    set(QINDAQT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
    function(qindaqt_enable_warnings target)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endfunction()
    # Standalone harness compiles the actual public storage implementation;
    # daemon/private headers and transport are deliberately absent here.
    add_library(qindaqt_keyring_storage STATIC
        "${QINDAQT_ROOT}/src/services/keyring/src/secure_buffer.cpp"
        "${QINDAQT_ROOT}/src/services/keyring/src/crypto.cpp"
        "${QINDAQT_ROOT}/src/services/keyring/src/format.cpp"
        "${QINDAQT_ROOT}/src/services/keyring/src/atomic_file.cpp"
        "${QINDAQT_ROOT}/src/services/keyring/src/collection_store.cpp")
    add_library(QindaQt::KeyringStorage ALIAS qindaqt_keyring_storage)
    target_include_directories(qindaqt_keyring_storage PUBLIC "${QINDAQT_ROOT}/src/services/keyring/include")
    target_link_libraries(qindaqt_keyring_storage PRIVATE OpenSSL::Crypto)
    target_compile_features(qindaqt_keyring_storage PUBLIC cxx_std_20)
    qindaqt_enable_warnings(qindaqt_keyring_storage)
    add_subdirectory("${QINDAQT_ROOT}/src/services/keyring_protocol" "${CMAKE_BINARY_DIR}/protocol")
    add_subdirectory("${QINDAQT_ROOT}/src/services/secret_portal" "${CMAKE_BINARY_DIR}/portal")
endif()
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)
foreach(behavior policy fd_wire broker)
    qt_add_executable(tst_secret_portal_${behavior} tst_${behavior}.cpp)
    target_link_libraries(tst_secret_portal_${behavior} PRIVATE QindaQt::SecretPortal Qt6::Test)
    target_compile_definitions(tst_secret_portal_${behavior} PRIVATE DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}")
    set_target_properties(tst_secret_portal_${behavior} PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(tst_secret_portal_${behavior})
    add_test(NAME secret_portal_${behavior} COMMAND tst_secret_portal_${behavior})
    set_tests_properties(secret_portal_${behavior} PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 60)
endforeach()

if(TARGET qindaqt-keyring AND TARGET xdg-desktop-portal-qindaqt)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME secret_portal_native_keyring COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_native_portal.py" "$<TARGET_FILE:qindaqt-keyring>"
        "$<TARGET_FILE:qindaqt-settings-service>" "$<TARGET_FILE:qindaqt_keyring_policy_compositor>"
        "$<TARGET_FILE:xdg-desktop-portal-qindaqt>" "${PROJECT_SOURCE_DIR}")
    set_tests_properties(secret_portal_native_keyring PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_FATAL_WARNINGS=1" LABELS "secret-portal;dbus;private-fixture")
endif()
