# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_network_protocol STATIC)
add_library(QindaQt::NetworkProtocol ALIAS qindaqt_network_protocol)

target_sources(
    qindaqt_network_protocol
    PRIVATE
        src/network_codec.cpp
        src/network_codec_p.h
        src/network_identity.cpp
        src/network_redaction.cpp
        src/network_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/network_protocol/network_codec.h
            include/qindaqt/services/network_protocol/network_identity.h
            include/qindaqt/services/network_protocol/network_intent.h
            include/qindaqt/services/network_protocol/network_limits.h
            include/qindaqt/services/network_protocol/network_redaction.h
            include/qindaqt/services/network_protocol/network_types.h
            include/qindaqt/services/network_protocol/network_validation.h
)
target_link_libraries(qindaqt_network_protocol PUBLIC Qt6::Core)
target_compile_features(qindaqt_network_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_network_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_network_protocol)

install(
    TARGETS qindaqt_network_protocol
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtNetworkN0
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtNetworkN0
)
