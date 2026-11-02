# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)

qt_add_library(qindaqt_link_transport STATIC)
add_library(QindaQt::LinkTransport ALIAS qindaqt_link_transport)

target_sources(
    qindaqt_link_transport
    PRIVATE
        src/desktop_pairing.cpp
        src/features.cpp
        src/hub.cpp
        src/identity.cpp
        src/private_dir.cpp
        src/session.cpp
        src/tls_listener.cpp
        src/trust_store.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_transport/desktop_pairing.h
            include/qindaqt/services/link_transport/feature.h
            include/qindaqt/services/link_transport/hub.h
            include/qindaqt/services/link_transport/identity.h
            include/qindaqt/services/link_transport/private_dir.h
            include/qindaqt/services/link_transport/ring_feature.h
            include/qindaqt/services/link_transport/session.h
            include/qindaqt/services/link_transport/status_feature.h
            include/qindaqt/services/link_transport/tls_listener.h
            include/qindaqt/services/link_transport/trust_store.h
)
target_link_libraries(
    qindaqt_link_transport
    PUBLIC QindaQt::LinkProtocol Qt6::Core Qt6::Network
    PRIVATE OpenSSL::Crypto
)
target_compile_features(qindaqt_link_transport PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_transport PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_transport)

install(
    TARGETS qindaqt_link_transport
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtLink
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtLink
)
