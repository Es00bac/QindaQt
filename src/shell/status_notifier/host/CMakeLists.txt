# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_status_notifier_host STATIC)
add_library(QindaQt::StatusNotifierHost ALIAS qindaqt_status_notifier_host)

target_sources(
    qindaqt_status_notifier_host
    PRIVATE
        src/status_notifier_host_registration.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/status_notifier/host/status_notifier_host_registration.h
)

target_link_libraries(
    qindaqt_status_notifier_host
    PUBLIC
        QindaQt::StatusNotifier
        Qt6::DBus
)
target_compile_features(qindaqt_status_notifier_host PUBLIC cxx_std_20)
set_target_properties(qindaqt_status_notifier_host PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_status_notifier_host)

install(
    TARGETS qindaqt_status_notifier_host
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
