# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_audio_client STATIC)
add_library(QindaQt::AudioClient ALIAS qindaqt_audio_client)

target_sources(
    qindaqt_audio_client
    PRIVATE
        src/audio_client.cpp
        src/audio_client_completion.cpp
        src/qt_audio_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/audio_client/audio_client.h
            include/qindaqt/services/audio_client/audio_transport.h
            include/qindaqt/services/audio_client/qt_audio_transport.h
)
target_link_libraries(
    qindaqt_audio_client PUBLIC QindaQt::AudioProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_audio_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_audio_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_audio_client)

install(
    TARGETS qindaqt_audio_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
