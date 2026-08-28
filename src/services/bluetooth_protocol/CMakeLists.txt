# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_bluetooth_protocol STATIC)
add_library(QindaQt::BluetoothProtocol ALIAS qindaqt_bluetooth_protocol)

target_sources(
    qindaqt_bluetooth_protocol
    PRIVATE
        src/bluetooth_dbus.cpp
        src/bluetooth_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/bluetooth_protocol/bluetooth_dbus.h
            include/qindaqt/services/bluetooth_protocol/bluetooth_limits.h
            include/qindaqt/services/bluetooth_protocol/bluetooth_types.h
            include/qindaqt/services/bluetooth_protocol/bluetooth_validation.h
)
target_link_libraries(qindaqt_bluetooth_protocol PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_bluetooth_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_bluetooth_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_bluetooth_protocol)

install(
    TARGETS qindaqt_bluetooth_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
