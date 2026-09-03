# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus WaylandClient)

qt_add_library(qindaqt_clipboard_service STATIC)
add_library(QindaQt::ClipboardService ALIAS qindaqt_clipboard_service)
target_sources(
    qindaqt_clipboard_service
    PRIVATE
        src/clipboard_host.cpp
        src/clipboard_service_object.cpp
        src/clipboard_service_object_p.h
        src/resident_clipboard_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/clipboard_service/clipboard_host.h
            include/qindaqt/services/clipboard_service/resident_clipboard_service.h
)
target_link_libraries(
    qindaqt_clipboard_service
    PUBLIC
        QindaQt::ClipboardModel
        QindaQt::ClipboardProtocol
        QindaQt::ClipboardWaylandAdapter
        Qt6::Core
        Qt6::DBus
)
target_compile_features(qindaqt_clipboard_service PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_clipboard_service PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_clipboard_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_clipboard_service)
install(
    TARGETS qindaqt_clipboard_service EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_executable(qindaqt-clipboard-host app/main.cpp)
target_link_libraries(
    qindaqt-clipboard-host
    PRIVATE
        QindaQt::ClipboardService
        QindaQt::SettingsClient
        QindaQt::SessionLockState
        Qt6::Core
        Qt6::DBus
)
target_compile_definitions(qindaqt-clipboard-host PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}")
set_target_properties(qindaqt-clipboard-host PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-clipboard-host)

configure_file(data/org.qindaqt.Clipboard1.service.in
               "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Clipboard1.service" @ONLY)
configure_file(data/qindaqt-clipboard-host.service.in
               "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-clipboard-host.service" @ONLY)
install(TARGETS qindaqt-clipboard-host RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Clipboard1.service"
        DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-clipboard-host.service"
        DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}")
install(FILES data/org.qindaqt.Clipboard1.xml
        DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces")
