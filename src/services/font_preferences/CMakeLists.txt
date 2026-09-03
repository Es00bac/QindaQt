# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui)

qt_add_library(qindaqt_font_preferences STATIC)
add_library(QindaQt::FontPreferences ALIAS qindaqt_font_preferences)

target_sources(
    qindaqt_font_preferences
    PRIVATE
        src/font_types.cpp
        src/font_catalog.cpp
        src/font_validation.cpp
        src/font_preferences.cpp
        src/font_preferences_codec.cpp
        src/font_bootstrap.cpp
        src/font_preferences_coordinator.cpp
        src/font_settings_bridge.cpp
        src/font_settings_bootstrap.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/font_preferences/font_types.h
            include/qindaqt/services/font_preferences/font_fact.h
            include/qindaqt/services/font_preferences/font_catalog.h
            include/qindaqt/services/font_preferences/font_validation.h
            include/qindaqt/services/font_preferences/font_preferences.h
            include/qindaqt/services/font_preferences/font_preferences_codec.h
            include/qindaqt/services/font_preferences/font_bootstrap.h
            include/qindaqt/services/font_preferences/font_preferences_coordinator.h
            include/qindaqt/services/font_preferences/font_settings_bridge.h
            include/qindaqt/services/font_preferences/font_settings_bootstrap.h
)
# AGENT-CONTRACT: font_preferences is fully transport-free: the F1 pre-window
# production composition (including Qt D-Bus) lives in font_discovery's
# font_session_bootstrap.cpp; the pure catalog/preference/coordinator/codec,
# bridge, and bootstrap helper sources import no Qt D-Bus (ADR-0047).
target_link_libraries(
    qindaqt_font_preferences
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE QindaQt::SettingsClient
)
target_compile_features(qindaqt_font_preferences PUBLIC cxx_std_20)
set_target_properties(qindaqt_font_preferences PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_font_preferences)

install(
    TARGETS qindaqt_font_preferences
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
