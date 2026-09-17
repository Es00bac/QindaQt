# SPDX-License-Identifier: LGPL-3.0-or-later

# Discovery, capability interrogation, polling, and serialized control for Wiz
# luminaires. Sockets and wall-clock time are injected seams, so the whole
# policy is exercised in tests without a network.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_wiz_client STATIC)
add_library(QindaQt::WizClient ALIAS qindaqt_wiz_client)

target_sources(
    qindaqt_wiz_client
    PRIVATE
        src/wiz_client.cpp
        src/wiz_client_operations.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/wiz_client/wiz_client.h
            include/qindaqt/services/wiz_client/wiz_transport.h
)
target_link_libraries(
    qindaqt_wiz_client PUBLIC QindaQt::WizModel QindaQt::WizProtocol Qt6::Core
)
target_compile_features(qindaqt_wiz_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_wiz_client PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_wiz_client)

install(
    TARGETS qindaqt_wiz_client
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtSmartLights
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtSmartLights
)
