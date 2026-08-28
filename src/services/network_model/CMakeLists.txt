# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_network_model STATIC)
add_library(QindaQt::NetworkModel ALIAS qindaqt_network_model)

target_sources(
    qindaqt_network_model
    PRIVATE
        src/network_intent_policy.cpp
        src/network_model.cpp
        src/network_scan_lease.cpp
        src/network_snapshot_gate.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/network_model/network_model.h
            include/qindaqt/services/network_model/network_model_state.h
            include/qindaqt/services/network_model/network_intent_policy.h
            include/qindaqt/services/network_model/network_scan_lease.h
            include/qindaqt/services/network_model/network_snapshot_gate.h
)
target_link_libraries(qindaqt_network_model
                      PUBLIC QindaQt::NetworkProtocol Qt6::Core)
target_compile_features(qindaqt_network_model PUBLIC cxx_std_20)
set_target_properties(qindaqt_network_model PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_network_model)

install(
    TARGETS qindaqt_network_model
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
