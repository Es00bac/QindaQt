# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_applet_host STATIC)

target_sources(
    qindaqt_applet_host
    PRIVATE
        src/capability_policy.cpp
        src/capability_policy_loader.cpp
        src/host_handshake.cpp
        src/host_lifecycle.cpp
        src/host_protocol_codec.cpp
        src/host_selection.cpp
        src/protocol_version.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/applet_host/capability_policy.h
            include/qindaqt/applet_host/capability_policy_loader.h
            include/qindaqt/applet_host/host_handshake.h
            include/qindaqt/applet_host/host_lifecycle.h
            include/qindaqt/applet_host/host_protocol_codec.h
            include/qindaqt/applet_host/host_selection.h
            include/qindaqt/applet_host/protocol_version.h
)

target_link_libraries(qindaqt_applet_host PUBLIC qindaqt_applets Qt6::Core)
target_compile_features(qindaqt_applet_host PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_applet_host)

install(
    TARGETS qindaqt_applet_host
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/applet-policy/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
)
