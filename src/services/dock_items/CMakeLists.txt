# SPDX-License-Identifier: LGPL-3.0-or-later

# Dock items (ADR-0265): the one bounded value and strict codec for
# `panels.dockItems`, and the confirmed-write helper other processes use to
# pin an application. Two targets keep the value free of Settings1 transport:
# the shell dock links only the pure value through the launcher runtime.

qt_add_library(qindaqt_dock_items STATIC)
add_library(QindaQt::DockItems ALIAS qindaqt_dock_items)
target_sources(
    qindaqt_dock_items
    PRIVATE
        src/dock_items.cpp
        src/dock_items_codec.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/dock_items/dock_items.h
)
# AGENT-CONTRACT: Qt Core plus the launcher's pure bounds only. No Settings1
# client, filesystem, process, D-Bus, or QML: paths are data here.
target_link_libraries(qindaqt_dock_items PUBLIC QindaQt::ShellLauncher Qt6::Core)
target_compile_features(qindaqt_dock_items PUBLIC cxx_std_20)
set_target_properties(qindaqt_dock_items
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_dock_items)

qt_add_library(qindaqt_dock_pins STATIC)
add_library(QindaQt::DockPins ALIAS qindaqt_dock_pins)
target_sources(
    qindaqt_dock_pins
    PRIVATE
        src/settings1_dock_pins.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/dock_items/settings1_dock_pins.h
)
target_link_libraries(qindaqt_dock_pins PUBLIC QindaQt::DockItems QindaQt::SettingsClient Qt6::Core)
target_compile_features(qindaqt_dock_pins PUBLIC cxx_std_20)
set_target_properties(qindaqt_dock_pins
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_dock_pins)

install(
    TARGETS qindaqt_dock_items qindaqt_dock_pins
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
