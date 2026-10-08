# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
qt_add_library(qindaqt_bluetooth_radio_client STATIC)
add_library(QindaQt::BluetoothRadioClient ALIAS qindaqt_bluetooth_radio_client)
target_sources(qindaqt_bluetooth_radio_client PRIVATE
    src/radio_types.cpp src/qt_radio_power_port.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/bluetooth_radio_helper/radio_types.h
    include/qindaqt/services/bluetooth_radio_helper/radio_power_port.h
    include/qindaqt/services/bluetooth_radio_helper/qt_radio_power_port.h)
target_link_libraries(qindaqt_bluetooth_radio_client PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_bluetooth_radio_client PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_bluetooth_radio_client PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_bluetooth_radio_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_bluetooth_radio_client)
install(TARGETS qindaqt_bluetooth_radio_client EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtBluetoothB1 FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT QindaQtBluetoothB1)

qt_add_library(qindaqt_bluetooth_radio_authority STATIC)
target_sources(qindaqt_bluetooth_radio_authority PRIVATE
    src/radio_operation.cpp src/radio_operation_p.h src/radio_platform_p.h
    src/radio_platform_linux.cpp src/qt_radio_authority.cpp src/qt_radio_authority_p.h
    src/radio_service_object.cpp src/radio_service_object_p.h)
target_link_libraries(qindaqt_bluetooth_radio_authority PUBLIC
    QindaQt::BluetoothRadioClient Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_bluetooth_radio_authority PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_bluetooth_radio_authority PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_bluetooth_radio_authority PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_bluetooth_radio_authority)
qt_add_executable(qindaqt-bluetooth-radio-helper app/main.cpp)
target_link_libraries(qindaqt-bluetooth-radio-helper PRIVATE qindaqt_bluetooth_radio_authority)
qindaqt_enable_warnings(qindaqt-bluetooth-radio-helper)
install(TARGETS qindaqt-bluetooth-radio-helper
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT QindaQtBluetoothB1)
configure_file(data/org.qindaqt.BluetoothRadio1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.BluetoothRadio1.service" @ONLY)
configure_file(data/qindaqt-bluetooth-radio-helper.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-bluetooth-radio-helper.service" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.BluetoothRadio1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}" COMPONENT QindaQtBluetoothB1)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-bluetooth-radio-helper.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}" COMPONENT QindaQtBluetoothB1)
