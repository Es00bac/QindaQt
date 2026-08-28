# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledAppShellConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml)

foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_APP_SHELL_LIBRARY
                          QINDAQT_APP_SHELL_LIBRARY_DIR QINDAQT_QML_STAGE_ROOT
                          QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

add_library(QindaQtInstalledAppShell SHARED IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledAppShell
    PROPERTIES
        IMPORTED_LOCATION "${QINDAQT_APP_SHELL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_link_libraries(QindaQtInstalledAppShell INTERFACE Qt6::Core Qt6::Gui Qt6::Qml)

add_executable(qindaqt_installed_app_shell_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_app_shell_consumer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_installed_app_shell_consumer
    PROPERTIES
        CXX_EXTENSIONS OFF
        BUILD_RPATH
            "${QINDAQT_APP_SHELL_LIBRARY_DIR};${QINDAQT_QML_STAGE_ROOT}/QindaQt/Controls;${QINDAQT_QML_STAGE_ROOT}/QindaQt/Tokens"
)
target_link_libraries(qindaqt_installed_app_shell_consumer PRIVATE QindaQtInstalledAppShell)
