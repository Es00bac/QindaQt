# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledDesignTokenConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui)

foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_DESIGN_TOKENS_LIBRARY
                          QINDAQT_THEMES_LIBRARY QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

add_library(QindaQtInstalledThemes STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledThemes
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_THEMES_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(QindaQtInstalledThemes INTERFACE Qt6::Core Qt6::Gui)

add_library(QindaQtInstalledDesignTokens STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledDesignTokens
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_DESIGN_TOKENS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(
    QindaQtInstalledDesignTokens
    INTERFACE QindaQtInstalledThemes Qt6::Core Qt6::Gui
)

add_executable(qindaqt_installed_design_token_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_design_token_consumer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_installed_design_token_consumer PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(
    qindaqt_installed_design_token_consumer PRIVATE QindaQtInstalledDesignTokens
)
