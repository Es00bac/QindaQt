# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_bluetooth_model STATIC)
add_library(QindaQt::BluetoothModel ALIAS qindaqt_bluetooth_model)

target_sources(
    qindaqt_bluetooth_model
    PRIVATE
        src/bluetooth_model.cpp
        src/deterministic_adapter_backend.cpp
        src/deterministic_backend_factory.cpp
        src/deterministic_adapter_backend.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/bluetooth_model/adapter_backend.h
            include/qindaqt/services/bluetooth_model/bluetooth_model.h
            include/qindaqt/services/bluetooth_model/deterministic_backend_factory.h
)
target_link_libraries(
    qindaqt_bluetooth_model
    PUBLIC QindaQt::BluetoothProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_bluetooth_model PUBLIC cxx_std_20)
set_target_properties(qindaqt_bluetooth_model PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_bluetooth_model)

# Qualification populates simulated platform state through the private
# deterministic backend header; production consumers receive only the empty
# factory-created backend.
target_include_directories(
    qindaqt_bluetooth_model
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(
    TARGETS qindaqt_bluetooth_model
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
