# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_settings_protocol STATIC)
add_library(QindaQt::SettingsProtocol ALIAS qindaqt_settings_protocol)

target_sources(
    qindaqt_settings_protocol
    PRIVATE
        src/settings_value_codec.cpp
        src/settings_wire_decode.cpp
        src/settings_wire_decode_p.h
        src/settings_wire_encode.cpp
        src/settings_wire_envelope.cpp
        src/settings_wire_status.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/settings_protocol/settings_value_codec.h
            include/qindaqt/services/settings_protocol/settings_wire_contract.h
            include/qindaqt/services/settings_protocol/settings_wire_decode.h
            include/qindaqt/services/settings_protocol/settings_wire_encode.h
            include/qindaqt/services/settings_protocol/settings_wire_status.h
)
target_link_libraries(
    qindaqt_settings_protocol PUBLIC Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_settings_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_settings_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_settings_protocol)

install(
    TARGETS qindaqt_settings_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
