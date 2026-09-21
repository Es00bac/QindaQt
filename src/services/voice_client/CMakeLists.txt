# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_voice_client STATIC)
add_library(QindaQt::VoiceClient ALIAS qindaqt_voice_client)

target_sources(
    qindaqt_voice_client
    PRIVATE
        src/voice_client.cpp
        src/voice_client_operations.cpp
        src/qt_voice_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/voice_client/qt_voice_transport.h
            include/qindaqt/services/voice_client/voice_client.h
            include/qindaqt/services/voice_client/voice_transport.h
)

# AGENT-CONTRACT: the client owns owner tracking, snapshot ordering and
# single-intent accounting. It must never link QML, the shell, audio capture, an
# input method, or a provider implementation; QtVoiceTransport is the only
# translation unit permitted to touch a session bus.
target_link_libraries(
    qindaqt_voice_client
    PUBLIC QindaQt::VoiceProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_voice_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_voice_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_voice_client)

install(
    TARGETS qindaqt_voice_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
