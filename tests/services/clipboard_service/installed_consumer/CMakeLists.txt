# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledClipboardConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_CLIPBOARD_PROTOCOL_LIBRARY
                          QINDAQT_CLIPBOARD_MODEL_LIBRARY QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

add_library(QindaQtInstalledClipboard STATIC IMPORTED GLOBAL)
set_target_properties(
    QindaQtInstalledClipboard
    PROPERTIES IMPORTED_LOCATION "${QINDAQT_CLIPBOARD_PROTOCOL_LIBRARY}"
               INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}")
target_link_libraries(QindaQtInstalledClipboard INTERFACE
                      "${QINDAQT_CLIPBOARD_MODEL_LIBRARY}" Qt6::Core Qt6::DBus)
add_executable(qindaqt_installed_clipboard_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_clipboard_consumer PRIVATE cxx_std_20)
set_target_properties(qindaqt_installed_clipboard_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(qindaqt_installed_clipboard_consumer PRIVATE QindaQtInstalledClipboard)
