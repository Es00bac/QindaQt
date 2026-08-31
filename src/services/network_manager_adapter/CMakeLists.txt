# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NetworkManager REQUIRED IMPORTED_TARGET libnm>=1.44)

qt_add_library(qindaqt_network_manager_adapter STATIC)
add_library(QindaQt::NetworkManagerAdapter ALIAS qindaqt_network_manager_adapter)

target_sources(
    qindaqt_network_manager_adapter
    PRIVATE
        src/libnm_network_manager_facts.cpp
        src/libnm_network_manager_port.cpp
        src/libnm_network_manager_port_p.h
        src/network_manager_backend.cpp
        src/network_manager_mapping.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/network_manager_adapter/network_manager_backend.h
            include/qindaqt/services/network_manager_adapter/network_manager_facts.h
            include/qindaqt/services/network_manager_adapter/network_manager_port.h
)
target_link_libraries(
    qindaqt_network_manager_adapter
    PUBLIC QindaQt::NetworkService Qt6::Core
    PRIVATE PkgConfig::NetworkManager
)
target_compile_features(qindaqt_network_manager_adapter PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_network_manager_adapter PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_network_manager_adapter PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_network_manager_adapter)

install(
    TARGETS qindaqt_network_manager_adapter
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtNetworkN1
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtNetworkN1
)

qt_add_executable(qindaqt-network-service app/main.cpp)
target_link_libraries(
    qindaqt-network-service
    PRIVATE QindaQt::NetworkManagerAdapter QindaQt::NetworkService Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-network-service PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-network-service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-network-service)

configure_file(
    data/org.qindaqt.Network1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Network1.service"
    @ONLY
)
configure_file(
    data/qindaqt-network-service.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-network-service.service"
    @ONLY
)

install(
    TARGETS qindaqt-network-service
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT QindaQtNetworkN1
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Network1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
    COMPONENT QindaQtNetworkN1
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-network-service.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
    COMPONENT QindaQtNetworkN1
)
install(
    FILES data/org.qindaqt.Network1.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
    COMPONENT QindaQtNetworkN1
)
