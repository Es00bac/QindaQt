# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_network_client STATIC)
add_library(QindaQt::NetworkClient ALIAS qindaqt_network_client)

target_sources(
    qindaqt_network_client
    PRIVATE src/network_client.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/network_client/network_client.h
            include/qindaqt/services/network_client/network_transport.h
)
target_link_libraries(qindaqt_network_client
                      PUBLIC QindaQt::NetworkModel Qt6::Core)
target_compile_features(qindaqt_network_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_network_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_network_client)

install(
    TARGETS qindaqt_network_client
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtNetworkN0
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtNetworkN0
)
