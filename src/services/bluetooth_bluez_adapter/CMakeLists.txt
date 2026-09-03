# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_bluez_adapter STATIC)
add_library(QindaQt::BluezAdapter ALIAS qindaqt_bluez_adapter)

target_sources(
    qindaqt_bluez_adapter
    PRIVATE
        src/bluez_adapter_backend.cpp
        src/bluez_backend_mode.cpp
        src/bluez_object_store.cpp
        src/bluez_object_store.h
        src/bluez_transport.cpp
        src/bluez_transport.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/bluetooth_bluez_adapter/bluez_adapter_backend.h
            include/qindaqt/services/bluetooth_bluez_adapter/bluez_backend_mode.h
)
target_link_libraries(
    qindaqt_bluez_adapter
    PUBLIC QindaQt::BluetoothModel Qt6::Core
    PRIVATE Qt6::DBus
)
target_compile_features(qindaqt_bluez_adapter PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_bluez_adapter PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_bluez_adapter PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_bluez_adapter)

install(
    TARGETS qindaqt_bluez_adapter
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtBluetoothB1
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtBluetoothB1
)
