# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_voice_protocol STATIC)
add_library(QindaQt::VoiceProtocol ALIAS qindaqt_voice_protocol)

target_sources(
    qindaqt_voice_protocol
    PRIVATE
        src/voice_types.cpp
        src/voice_dbus.cpp
        src/voice_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/voice_protocol/voice_dbus.h
            include/qindaqt/services/voice_protocol/voice_limits.h
            include/qindaqt/services/voice_protocol/voice_settings_keys.h
            include/qindaqt/services/voice_protocol/voice_types.h
            include/qindaqt/services/voice_protocol/voice_validation.h
)

# AGENT-CONTRACT: the Voice1 wire contract owns its own bounds and marshalling
# and nothing else. It must never link a transport, a provider, a model, QML,
# or any audio or input-method library; both the shell client and the offline
# tests depend on it staying free of a session bus.
target_link_libraries(qindaqt_voice_protocol PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_voice_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_voice_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_voice_protocol)

install(
    TARGETS qindaqt_voice_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
