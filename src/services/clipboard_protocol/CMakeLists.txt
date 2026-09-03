# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_clipboard_protocol STATIC)
add_library(QindaQt::ClipboardProtocol ALIAS qindaqt_clipboard_protocol)

target_sources(
    qindaqt_clipboard_protocol
    PRIVATE
        src/clipboard_dbus.cpp
        src/clipboard_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/clipboard_protocol/clipboard_dbus.h
            include/qindaqt/services/clipboard_protocol/clipboard_protocol.h
            include/qindaqt/services/clipboard_protocol/clipboard_validation.h
)
target_link_libraries(
    qindaqt_clipboard_protocol
    PUBLIC QindaQt::ClipboardModel Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_clipboard_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_clipboard_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_clipboard_protocol)

install(
    TARGETS qindaqt_clipboard_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
