# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledFontConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui DBus)

foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_FONT_PREFERENCES_LIBRARY
                          QINDAQT_SETTINGS_CLIENT_LIBRARY QINDAQT_SETTINGS_PROTOCOL_LIBRARY
                          QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

add_library(QindaQtInstalledSettingsProtocol STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledSettingsProtocol
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_SETTINGS_PROTOCOL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(QindaQtInstalledSettingsProtocol INTERFACE Qt6::Core Qt6::DBus)

add_library(QindaQtInstalledSettingsClient STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledSettingsClient
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_SETTINGS_CLIENT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(
    QindaQtInstalledSettingsClient
    INTERFACE QindaQtInstalledSettingsProtocol Qt6::Core Qt6::DBus
)

add_library(QindaQtInstalledFontPreferences STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledFontPreferences
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_FONT_PREFERENCES_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(
    QindaQtInstalledFontPreferences
    INTERFACE QindaQtInstalledSettingsClient Qt6::Core Qt6::Gui
)

add_executable(qindaqt_installed_font_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_font_consumer PRIVATE cxx_std_20)
set_target_properties(qindaqt_installed_font_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(qindaqt_installed_font_consumer PRIVATE QindaQtInstalledFontPreferences)
