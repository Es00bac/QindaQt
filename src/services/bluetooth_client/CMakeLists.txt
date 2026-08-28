# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_bluetooth_client STATIC)
add_library(QindaQt::BluetoothClient ALIAS qindaqt_bluetooth_client)

target_sources(
    qindaqt_bluetooth_client
    PRIVATE
        src/bluetooth_client.cpp
        src/bluetooth_client_completion.cpp
        src/qt_bluetooth_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/bluetooth_client/bluetooth_client.h
            include/qindaqt/services/bluetooth_client/bluetooth_transport.h
            include/qindaqt/services/bluetooth_client/qt_bluetooth_transport.h
)
target_link_libraries(
    qindaqt_bluetooth_client PUBLIC QindaQt::BluetoothProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_bluetooth_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_bluetooth_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_bluetooth_client)

install(
    TARGETS qindaqt_bluetooth_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
