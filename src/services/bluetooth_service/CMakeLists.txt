# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_bluetooth_service STATIC)
add_library(QindaQt::BluetoothService ALIAS qindaqt_bluetooth_service)

target_sources(
    qindaqt_bluetooth_service
    PRIVATE
        src/bluetooth_service_object.cpp
        src/bluetooth_service_object_p.h
        src/resident_bluetooth_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/bluetooth_service/resident_bluetooth_service.h
)
target_link_libraries(
    qindaqt_bluetooth_service
    PUBLIC QindaQt::BluetoothModel QindaQt::BluetoothProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_bluetooth_service PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_bluetooth_service PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_bluetooth_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_bluetooth_service)

install(
    TARGETS qindaqt_bluetooth_service
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_executable(qindaqt-bluetooth-service app/main.cpp)
target_link_libraries(
    qindaqt-bluetooth-service
    PRIVATE QindaQt::BluetoothService QindaQt::BluetoothModel Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-bluetooth-service PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-bluetooth-service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-bluetooth-service)

configure_file(
    data/org.qindaqt.Bluetooth1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Bluetooth1.service"
    @ONLY
)
configure_file(
    data/qindaqt-bluetooth-service.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-bluetooth-service.service"
    @ONLY
)

install(TARGETS qindaqt-bluetooth-service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Bluetooth1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-bluetooth-service.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
)
install(
    FILES data/org.qindaqt.Bluetooth1.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
)
