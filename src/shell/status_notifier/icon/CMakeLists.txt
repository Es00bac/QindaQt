# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Gui)

qt_add_library(qindaqt_status_notifier_icon STATIC)
add_library(QindaQt::StatusNotifierIcon ALIAS qindaqt_status_notifier_icon)

target_sources(
    qindaqt_status_notifier_icon
    PRIVATE
        src/status_notifier_icon_locator.cpp
        src/status_notifier_icon_renderer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/status_notifier/icon/status_notifier_icon_locator.h
            include/qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h
)

target_link_libraries(
    qindaqt_status_notifier_icon
    PUBLIC
        QindaQt::StatusNotifier
        Qt6::Gui
)
target_compile_features(qindaqt_status_notifier_icon PUBLIC cxx_std_20)
set_target_properties(qindaqt_status_notifier_icon PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_status_notifier_icon)

install(
    TARGETS qindaqt_status_notifier_icon
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
