# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_firewall_protocol STATIC)
add_library(QindaQt::FirewallProtocol ALIAS qindaqt_firewall_protocol)

target_sources(
    qindaqt_firewall_protocol
    PRIVATE
        src/firewall_codec.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/firewall_protocol/firewall_codec.h
            include/qindaqt/services/firewall_protocol/firewall_types.h
)
target_link_libraries(qindaqt_firewall_protocol PUBLIC Qt6::Core)
target_compile_features(qindaqt_firewall_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_firewall_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_firewall_protocol)

install(
    TARGETS qindaqt_firewall_protocol
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtFirewallApplet
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtFirewallApplet
)
