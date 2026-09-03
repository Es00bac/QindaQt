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

# AGENT-CONTRACT: font_preferences is fully transport-free. The F1 pre-window
# production composition (the only Qt D-Bus consumer in the font stack) lives
# in font_discovery/src/font_session_bootstrap.cpp; no font_preferences source
# may import Qt D-Bus (ADR-0047).
foreach(source IN LISTS font_sources)
    file(READ "${source}" content)
    if(content MATCHES "qindaqt/services/display_" OR content MATCHES "QindaQt::Display"
       OR content MATCHES "<QtQml/" OR content MATCHES "<QtQuick/"
       OR content MATCHES "fontconfig/"
       OR content MATCHES "<QtCore/QProcess" OR content MATCHES "<QtCore/QThread")
        message(FATAL_ERROR "Forbidden font-preferences dependency in ${source}")
    endif()
    if(content MATCHES "<QtDBus/" OR content MATCHES "<QDBus")
        message(FATAL_ERROR "Qt D-Bus import inside the pure font-preferences module: ${source}")
    endif()
endforeach()

message(STATUS "Font preferences boundary is pure, display-transport and presentation independent")
