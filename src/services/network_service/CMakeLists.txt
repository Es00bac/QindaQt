# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_network_service STATIC)
add_library(QindaQt::NetworkService ALIAS qindaqt_network_service)

target_sources(
    qindaqt_network_service
    PRIVATE
        src/network_service_coordinator.cpp
        src/network_service_object.cpp
        src/network_service_object_p.h
        src/resident_network_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/network_service/network_backend.h
            include/qindaqt/services/network_service/network_service_coordinator.h
            include/qindaqt/services/network_service/resident_network_service.h
)
target_link_libraries(
    qindaqt_network_service
    PUBLIC QindaQt::NetworkModel QindaQt::NetworkProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_network_service PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_network_service PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_network_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_network_service)

install(
    TARGETS qindaqt_network_service
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtNetworkN1
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtNetworkN1
)
