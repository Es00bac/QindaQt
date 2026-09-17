# SPDX-License-Identifier: LGPL-3.0-or-later

# The only component in the smart-light stack that owns a socket.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Network)

qt_add_library(qindaqt_wiz_udp_transport STATIC)
add_library(QindaQt::WizUdpTransport ALIAS qindaqt_wiz_udp_transport)

target_sources(
    qindaqt_wiz_udp_transport
    PRIVATE src/wiz_udp_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/services/wiz_udp_transport/wiz_udp_transport.h
)
target_link_libraries(
    qindaqt_wiz_udp_transport PUBLIC QindaQt::WizClient Qt6::Core Qt6::Network
)
target_compile_features(qindaqt_wiz_udp_transport PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_wiz_udp_transport PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_wiz_udp_transport)

install(
    TARGETS qindaqt_wiz_udp_transport
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtSmartLights
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtSmartLights
)
