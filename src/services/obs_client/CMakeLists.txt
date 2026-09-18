# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS WebSockets)

# The desktop's obs-websocket v5 client. The protocol half is pure and the
# transport is a seam, so every rule is testable without OBS running.
qt_add_library(qindaqt_obs_client STATIC)
add_library(QindaQt::ObsClient ALIAS qindaqt_obs_client)

target_sources(
    qindaqt_obs_client
    PRIVATE
        src/obs_client.cpp
        src/obs_protocol.cpp
        src/obs_provisioning.cpp
        src/obs_secret_store.cpp
        src/qt_obs_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/obs_client/obs_client.h
            include/qindaqt/services/obs_client/obs_protocol.h
            include/qindaqt/services/obs_client/obs_provisioning.h
            include/qindaqt/services/obs_client/obs_secret_store.h
            include/qindaqt/services/obs_client/obs_transport.h
            include/qindaqt/services/obs_client/obs_types.h
            include/qindaqt/services/obs_client/qt_obs_transport.h
)
target_link_libraries(
    qindaqt_obs_client PUBLIC Qt6::Core Qt6::DBus Qt6::WebSockets
)
target_compile_features(qindaqt_obs_client PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_obs_client PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_obs_client)

install(
    TARGETS qindaqt_obs_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
