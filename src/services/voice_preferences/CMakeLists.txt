# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_voice_preferences STATIC)
add_library(QindaQt::VoicePreferences ALIAS qindaqt_voice_preferences)

target_sources(qindaqt_voice_preferences
    PRIVATE src/voice_input_preference_gate.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/services/voice_preferences/voice_input_preference_gate.h)
target_link_libraries(qindaqt_voice_preferences
    PUBLIC QindaQt::SettingsClient QindaQt::VoiceProtocol Qt6::Core)
target_compile_features(qindaqt_voice_preferences PUBLIC cxx_std_20)
set_target_properties(qindaqt_voice_preferences PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_voice_preferences)

install(TARGETS qindaqt_voice_preferences EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION include)
