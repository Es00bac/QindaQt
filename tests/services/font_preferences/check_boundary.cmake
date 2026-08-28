# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(
    GLOB_RECURSE font_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/font_preferences/*.h"
    "${SOURCE_ROOT}/src/services/font_preferences/*.cpp"
)
list(APPEND font_sources "${SOURCE_ROOT}/src/services/font_preferences/CMakeLists.txt")

foreach(source IN LISTS font_sources)
    file(READ "${source}" content)
    if(content MATCHES "qindaqt/services/display_" OR content MATCHES "QindaQt::Display"
       OR content MATCHES "<QtDBus/" OR content MATCHES "<QtQml/"
       OR content MATCHES "<QtQuick/" OR content MATCHES "fontconfig/"
       OR content MATCHES "<QtCore/QProcess" OR content MATCHES "<QtCore/QThread")
        message(FATAL_ERROR "Forbidden font-preferences dependency in ${source}")
    endif()
endforeach()

message(STATUS "Font preferences boundary is pure, display-transport and presentation independent")
