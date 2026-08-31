# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_network_qt_transport STATIC)
add_library(QindaQt::NetworkQtTransport ALIAS qindaqt_network_qt_transport)

target_sources(
    qindaqt_network_qt_transport
    PRIVATE src/qt_network_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/network_qt_transport/qt_network_transport.h
)
target_link_libraries(
    qindaqt_network_qt_transport
    PUBLIC QindaQt::NetworkClient Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_network_qt_transport PUBLIC cxx_std_20)
set_target_properties(qindaqt_network_qt_transport PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_network_qt_transport)

install(
    TARGETS qindaqt_network_qt_transport
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtNetworkN1
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtNetworkN1
)
