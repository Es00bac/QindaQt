# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_status_notifier_watcher STATIC)
add_library(QindaQt::StatusNotifierWatcher ALIAS qindaqt_status_notifier_watcher)

target_sources(
    qindaqt_status_notifier_watcher
    PRIVATE
        src/status_notifier_watcher_object.cpp
        src/status_notifier_watcher_object_p.h
        src/status_notifier_watcher_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h
)

target_link_libraries(
    qindaqt_status_notifier_watcher
    PUBLIC
        QindaQt::StatusNotifier
        Qt6::DBus
)
target_compile_features(qindaqt_status_notifier_watcher PUBLIC cxx_std_20)
set_target_properties(qindaqt_status_notifier_watcher PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_status_notifier_watcher)

install(
    TARGETS qindaqt_status_notifier_watcher
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
