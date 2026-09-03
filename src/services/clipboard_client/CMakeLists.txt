# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_clipboard_client STATIC)
add_library(QindaQt::ClipboardClient ALIAS qindaqt_clipboard_client)
target_sources(
    qindaqt_clipboard_client
    PRIVATE
        src/clipboard_client.cpp
        src/qt_clipboard_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/clipboard_client/clipboard_client.h
            include/qindaqt/services/clipboard_client/clipboard_transport.h
            include/qindaqt/services/clipboard_client/qt_clipboard_transport.h
)
target_link_libraries(
    qindaqt_clipboard_client PUBLIC QindaQt::ClipboardProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_clipboard_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_clipboard_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_clipboard_client)
install(
    TARGETS qindaqt_clipboard_client EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
