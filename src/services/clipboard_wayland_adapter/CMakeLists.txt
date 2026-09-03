# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)
enable_language(C)

qt_add_library(qindaqt_clipboard_wayland_adapter STATIC)
add_library(QindaQt::ClipboardWaylandAdapter ALIAS qindaqt_clipboard_wayland_adapter)

qt6_generate_wayland_protocol_client_sources(
    qindaqt_clipboard_wayland_adapter
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocol/ext-data-control-v1.xml"
    PRIVATE_CODE
)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/qwayland-ext-data-control-v1.cpp"
    PROPERTIES COMPILE_OPTIONS "-Wno-sign-conversion;-Wno-shadow"
)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-ext-data-control-v1-protocol.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion;-Wno-shadow"
)

target_sources(
    qindaqt_clipboard_wayland_adapter
    PRIVATE
        src/qt_wayland_clipboard_adapter.cpp
        src/qt_wayland_clipboard_sources_p.cpp
        src/qt_wayland_clipboard_sources_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h
            include/qindaqt/services/clipboard_wayland_adapter/production_clipboard_wayland_adapter.h
)
target_link_libraries(
    qindaqt_clipboard_wayland_adapter
    PUBLIC QindaQt::ClipboardModel Qt6::Core
    PRIVATE Qt6::WaylandClient Wayland::Client
)
target_compile_features(qindaqt_clipboard_wayland_adapter PUBLIC cxx_std_20)
set_target_properties(qindaqt_clipboard_wayland_adapter PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_clipboard_wayland_adapter)

install(
    TARGETS qindaqt_clipboard_wayland_adapter
    EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
