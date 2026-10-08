# SPDX-License-Identifier: LGPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QINDAQT_VOICE_DBUS REQUIRED IMPORTED_TARGET GLOBAL dbus-1)
qt_add_library(qindaqt_voice_configuration STATIC)
add_library(QindaQt::VoiceConfiguration ALIAS qindaqt_voice_configuration)
target_sources(qindaqt_voice_configuration PRIVATE
    src/voice_configuration_codec.cpp
    src/voice_configuration_client.cpp
    src/qt_voice_configuration_transport.cpp
    src/native_voice_wire.cpp src/native_voice_wire_p.h
    src/native_voice_codec.cpp src/native_voice_codec_p.h
    include/qindaqt/services/voice_configuration/voice_configuration_transport.h
    include/qindaqt/services/voice_configuration/voice_configuration_client.h
    include/qindaqt/services/voice_configuration/qt_voice_configuration_transport.h
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/voice_configuration/voice_configuration_types.h
    include/qindaqt/services/voice_configuration/voice_configuration_codec.h
    include/qindaqt/services/voice_configuration/voice_configuration_transport.h
    include/qindaqt/services/voice_configuration/voice_configuration_client.h
    include/qindaqt/services/voice_configuration/qt_voice_configuration_transport.h)
target_link_libraries(qindaqt_voice_configuration PUBLIC Qt6::Core Qt6::DBus PRIVATE PkgConfig::QINDAQT_VOICE_DBUS)
target_compile_features(qindaqt_voice_configuration PUBLIC cxx_std_20)
set_target_properties(qindaqt_voice_configuration PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_voice_configuration)
install(TARGETS qindaqt_voice_configuration EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
