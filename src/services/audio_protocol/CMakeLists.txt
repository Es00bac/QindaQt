# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_audio_protocol STATIC)
add_library(QindaQt::AudioProtocol ALIAS qindaqt_audio_protocol)

target_sources(
    qindaqt_audio_protocol
    PRIVATE
        src/audio_dbus.cpp
        src/audio_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/audio_protocol/audio_dbus.h
            include/qindaqt/services/audio_protocol/audio_limits.h
            include/qindaqt/services/audio_protocol/audio_types.h
            include/qindaqt/services/audio_protocol/audio_validation.h
)
target_link_libraries(qindaqt_audio_protocol PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_audio_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_audio_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_audio_protocol)

install(
    TARGETS qindaqt_audio_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
