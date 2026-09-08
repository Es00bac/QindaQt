# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_status_notifier_item_client STATIC)
add_library(QindaQt::StatusNotifierItemClient ALIAS qindaqt_status_notifier_item_client)

target_sources(
    qindaqt_status_notifier_item_client
    PRIVATE
        src/status_notifier_item_client.cpp
        src/status_notifier_item_signals.cpp
        src/status_notifier_item_monitor.cpp
        src/status_notifier_item_monitor_menu.cpp
        src/status_notifier_item_menu.cpp
        src/status_notifier_item_menu.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h
            include/qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h
)

target_link_libraries(
    qindaqt_status_notifier_item_client
    PUBLIC
        QindaQt::StatusNotifier
        Qt6::DBus
    PRIVATE QindaQt::GlobalMenuDbusMenu
)
target_compile_features(qindaqt_status_notifier_item_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_status_notifier_item_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_status_notifier_item_client)

install(
    TARGETS qindaqt_status_notifier_item_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
