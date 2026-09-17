# SPDX-License-Identifier: LGPL-3.0-or-later

# Pure values, codec, capability inference, and admission rules for the Wiz
# LAN control protocol. This target performs no I/O and knows nothing about
# sockets, timers, or the shell: everything here is testable from a byte array.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_wiz_protocol STATIC)
add_library(QindaQt::WizProtocol ALIAS qindaqt_wiz_protocol)

target_sources(
    qindaqt_wiz_protocol
    PRIVATE
        src/wiz_capabilities.cpp
        src/wiz_messages_decode.cpp
        src/wiz_messages_encode.cpp
        src/wiz_scenes.cpp
        src/wiz_types.cpp
        src/wiz_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/wiz_protocol/wiz_capabilities.h
            include/qindaqt/services/wiz_protocol/wiz_limits.h
            include/qindaqt/services/wiz_protocol/wiz_messages.h
            include/qindaqt/services/wiz_protocol/wiz_scenes.h
            include/qindaqt/services/wiz_protocol/wiz_types.h
            include/qindaqt/services/wiz_protocol/wiz_validation.h
)
target_link_libraries(qindaqt_wiz_protocol PUBLIC Qt6::Core)
target_compile_features(qindaqt_wiz_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_wiz_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_wiz_protocol)

install(
    TARGETS qindaqt_wiz_protocol
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtSmartLights
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtSmartLights
)
