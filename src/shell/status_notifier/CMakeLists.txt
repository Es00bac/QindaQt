# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_status_notifier STATIC)
add_library(QindaQt::StatusNotifier ALIAS qindaqt_status_notifier)

target_sources(
    qindaqt_status_notifier
    PRIVATE
        src/status_notifier_presentation.cpp
        src/status_notifier_registry.cpp
        src/status_notifier_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/status_notifier/status_notifier_limits.h
            include/qindaqt/shell/status_notifier/status_notifier_presentation.h
            include/qindaqt/shell/status_notifier/status_notifier_registry.h
            include/qindaqt/shell/status_notifier/status_notifier_types.h
            include/qindaqt/shell/status_notifier/status_notifier_validation.h
            include/qindaqt/shell/status_notifier/status_notifier_transport.h
)
target_link_libraries(qindaqt_status_notifier PUBLIC Qt6::Core)
target_compile_features(qindaqt_status_notifier PUBLIC cxx_std_20)
set_target_properties(qindaqt_status_notifier PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_status_notifier)

install(
    TARGETS qindaqt_status_notifier
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
