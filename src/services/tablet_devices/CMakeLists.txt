# SPDX-License-Identifier: GPL-3.0-or-later

# Tablet device boundary shared by the session process (which maps a pen
# display to its own screen and announces it) and the Settings Input route
# (which presents every knob). Policy here is pure; the KWin D-Bus adapter is
# the only transport-aware file.
qt_add_library(qindaqt_tablet_devices STATIC)
add_library(QindaQt::TabletDevices ALIAS qindaqt_tablet_devices)

target_sources(
    qindaqt_tablet_devices
    PRIVATE
        src/kwin_tablet_device_port.cpp
        src/tablet_device_port.cpp
        src/tablet_geometry.cpp
        src/tablet_mapping_ledger.cpp
        src/tablet_mapping_store.cpp
        src/tablet_output_inventory.cpp
        src/tablet_output_matcher.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/tablet_devices/kwin_tablet_devices.h
            include/qindaqt/services/tablet_devices/tablet_device_port.h
            include/qindaqt/services/tablet_devices/tablet_geometry.h
            include/qindaqt/services/tablet_devices/tablet_mapping_ledger.h
            include/qindaqt/services/tablet_devices/tablet_mapping_store.h
            include/qindaqt/services/tablet_devices/tablet_output_inventory.h
            include/qindaqt/services/tablet_devices/tablet_output_matcher.h
)
target_link_libraries(
    qindaqt_tablet_devices PUBLIC QindaQt::SettingsClient Qt6::Core Qt6::DBus Qt6::Gui
)
target_compile_features(qindaqt_tablet_devices PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_tablet_devices PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_tablet_devices)

install(
    TARGETS qindaqt_tablet_devices
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
