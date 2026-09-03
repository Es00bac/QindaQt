# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client Cursor)

qt_add_executable(qindaqt_clipboard_service_tests tst_clipboard_service.cpp support/fake_wayland_adapter.h)
target_link_libraries(qindaqt_clipboard_service_tests PRIVATE QindaQt::ClipboardService Qt6::Test)
qindaqt_enable_warnings(qindaqt_clipboard_service_tests)
add_test(NAME qindaqt.clipboard-service COMMAND qindaqt_clipboard_service_tests)
set_tests_properties(qindaqt.clipboard-service PROPERTIES LABELS "service;clipboard" TIMEOUT 20)

qt_add_executable(qindaqt_clipboard_settings_consent_tests
    tst_clipboard_settings_consent.cpp
    "${CMAKE_SOURCE_DIR}/src/services/clipboard_service/app/clipboard_history_consent.cpp")
target_include_directories(qindaqt_clipboard_settings_consent_tests
    PRIVATE "${CMAKE_SOURCE_DIR}/src/services/clipboard_service/app")
target_link_libraries(qindaqt_clipboard_settings_consent_tests
    PRIVATE QindaQt::SettingsClient Qt6::Test)
qindaqt_enable_warnings(qindaqt_clipboard_settings_consent_tests)
add_test(NAME qindaqt.clipboard-settings-consent
    COMMAND qindaqt_clipboard_settings_consent_tests)
set_tests_properties(qindaqt.clipboard-settings-consent
    PROPERTIES LABELS "service;clipboard;settings;privacy" TIMEOUT 20)

find_program(QINDAQT_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)
qt_add_executable(qindaqt_clipboard_private_bus_tests
    tst_clipboard_private_bus.cpp
    support/fake_wayland_adapter.h
    support/private_bus.h)
target_compile_definitions(qindaqt_clipboard_private_bus_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}")
target_link_libraries(qindaqt_clipboard_private_bus_tests
    PRIVATE QindaQt::ClipboardService QindaQt::ClipboardClient Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_clipboard_private_bus_tests)
add_test(NAME qindaqt.clipboard-private-bus COMMAND qindaqt_clipboard_private_bus_tests)
set_tests_properties(qindaqt.clipboard-private-bus PROPERTIES LABELS "service;clipboard;dbus" TIMEOUT 30 RUN_SERIAL TRUE)

qt_add_executable(qindaqt_clipboard_request_cache_tests
    tst_clipboard_request_cache.cpp
    support/fake_wayland_adapter.h
    support/private_bus.h)
target_compile_definitions(qindaqt_clipboard_request_cache_tests
    PRIVATE QINDAQT_DBUS_DAEMON_EXECUTABLE="${QINDAQT_DBUS_DAEMON_EXECUTABLE}")
target_link_libraries(qindaqt_clipboard_request_cache_tests
    PRIVATE QindaQt::ClipboardService QindaQt::ClipboardClient Qt6::DBus Qt6::Test)
qindaqt_enable_warnings(qindaqt_clipboard_request_cache_tests)
add_test(NAME qindaqt.clipboard-request-cache
    COMMAND qindaqt_clipboard_request_cache_tests)
set_tests_properties(qindaqt.clipboard-request-cache
    PROPERTIES LABELS "service;clipboard;dbus;hostile" TIMEOUT 30 RUN_SERIAL TRUE)

add_test(
    NAME qindaqt.clipboard-service-boundary-poison
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.clipboard-service-boundary-poison PROPERTIES LABELS "service;clipboard;policy;hostile" TIMEOUT 20)

# AGENT-CONTRACT: Stage a clean install and link a consumer against only the
# installed Clipboard1/model archives and public headers. Runtime activation
# remains covered separately by the private-bus row.
add_test(
    NAME qindaqt.clipboard-staged-install
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/staged-install"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/installed_consumer.cpp"
        "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_INSTALL_SYSTEMDUSERUNITDIR=${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
        "-DQINDAQT_CLIPBOARD_PROTOCOL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_clipboard_protocol>"
        "-DQINDAQT_CLIPBOARD_MODEL_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_clipboard_model>"
        "-DQINDAQT_CLIPBOARD_CLIENT_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_clipboard_client>"
        "-DQINDAQT_CLIPBOARD_ADAPTER_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_clipboard_wayland_adapter>"
        "-DQINDAQT_CLIPBOARD_SERVICE_LIBRARY_NAME=$<TARGET_FILE_NAME:qindaqt_clipboard_service>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_staged_install.cmake"
)
set_tests_properties(qindaqt.clipboard-staged-install PROPERTIES LABELS "package;clipboard")
