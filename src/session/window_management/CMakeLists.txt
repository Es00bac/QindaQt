# SPDX-License-Identifier: GPL-3.0-or-later

# The windowManagement.* live bridge (ADR-0209): Settings1 -> kwinrc -> KWin
# reconfigure, hosted by qindaqt-session. Session-side only: it never links
# KWin, the shell, or the compositor plugin.

find_package(KF6Config 6.0 REQUIRED CONFIG)

qt_add_library(qindaqt_window_management_bridge STATIC)
add_library(QindaQt::WindowManagementBridge ALIAS qindaqt_window_management_bridge)
target_sources(
    qindaqt_window_management_bridge
    PRIVATE
        src/kwin_reconfigure_requester.cpp
        src/kwin_window_management_writer.cpp
        src/window_management_bridge.cpp
        src/window_management_preferences.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/session/window_management/kwin_reconfigure_requester.h
            include/qindaqt/session/window_management/kwin_window_management_writer.h
            include/qindaqt/session/window_management/window_management_bridge.h
            include/qindaqt/session/window_management/window_management_preferences.h
)
target_link_libraries(
    qindaqt_window_management_bridge
    PUBLIC
        QindaQt::SettingsClient
        KF6::ConfigCore
        Qt6::Core
        Qt6::DBus
)
target_compile_features(qindaqt_window_management_bridge PUBLIC cxx_std_20)
set_target_properties(qindaqt_window_management_bridge PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_window_management_bridge)
