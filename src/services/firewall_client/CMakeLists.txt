# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_firewall_client STATIC)
add_library(QindaQt::FirewallClient ALIAS qindaqt_firewall_client)

target_sources(
    qindaqt_firewall_client
    PRIVATE
        src/firewall_client.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/firewall_client/firewall_client.h
            include/qindaqt/services/firewall_client/firewall_transport.h
)
target_link_libraries(qindaqt_firewall_client PUBLIC QindaQt::FirewallProtocol Qt6::Core)
target_compile_features(qindaqt_firewall_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_firewall_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_firewall_client)

install(
    TARGETS qindaqt_firewall_client
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtFirewallApplet
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtFirewallApplet
)
