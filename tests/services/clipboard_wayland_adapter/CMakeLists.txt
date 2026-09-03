# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client Cursor Server)
find_package(WaylandScanner REQUIRED)
enable_language(C)

qt_add_executable(qindaqt_clipboard_wayland_adapter_tests
    tst_clipboard_wayland_adapter.cpp
    support/fake_data_control_server.cpp
    support/fake_data_control_server.h
)
ecm_add_wayland_server_protocol(
    qindaqt_clipboard_wayland_adapter_tests
    PROTOCOL "${CMAKE_SOURCE_DIR}/src/services/clipboard_wayland_adapter/protocol/ext-data-control-v1.xml"
    BASENAME ext-data-control-v1
    PRIVATE_CODE
)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-ext-data-control-v1-protocol.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion;-Wno-shadow"
)
target_link_libraries(qindaqt_clipboard_wayland_adapter_tests
    PRIVATE QindaQt::ClipboardWaylandAdapter Wayland::Server Qt6::Test)
target_include_directories(qindaqt_clipboard_wayland_adapter_tests
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
qindaqt_enable_warnings(qindaqt_clipboard_wayland_adapter_tests)
add_test(NAME qindaqt.clipboard-wayland-adapter COMMAND qindaqt_clipboard_wayland_adapter_tests)
set_tests_properties(qindaqt.clipboard-wayland-adapter PROPERTIES LABELS "service;clipboard;wayland" TIMEOUT 30)

add_test(
    NAME qindaqt.clipboard-wayland-boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DMODULE_SOURCE_DIR=${CMAKE_SOURCE_DIR}/src/services/clipboard_wayland_adapter"
        "-DEXPECTED_XML_SHA256=293deba5dfdb974fbcf8dcf891a30a51a36a8bba0047dc6bdf6a03194089b7fb"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.clipboard-wayland-boundary PROPERTIES LABELS "service;clipboard;wayland;policy" TIMEOUT 20)
