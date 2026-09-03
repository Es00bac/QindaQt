# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

# AGENT-CONTRACT: ADR-0057 confines fontconfig to src/services/font_discovery.
# No other product source may include fontconfig headers or link the
# fontconfig target; consumers receive plain FontFact values.
file(
    GLOB_RECURSE product_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/*.h"
    "${SOURCE_ROOT}/src/*.cpp"
    "${SOURCE_ROOT}/src/*.cmake"
    "${SOURCE_ROOT}/src/CMakeLists.txt"
    "${SOURCE_ROOT}/src/*/CMakeLists.txt"
    "${SOURCE_ROOT}/src/*/*/CMakeLists.txt"
    "${SOURCE_ROOT}/src/*/*/*/CMakeLists.txt"
    "${SOURCE_ROOT}/src/*/*/*/*/CMakeLists.txt"
)

set(fontconfig_root "${SOURCE_ROOT}/src/services/font_discovery")

foreach(source IN LISTS product_sources)
    if(source MATCHES "^${fontconfig_root}/")
        continue()
    endif()
    file(READ "${source}" content)
    if(content MATCHES "fontconfig/" OR content MATCHES "Fontconfig::"
       OR content MATCHES "PkgConfig::Fontconfig" OR content MATCHES "FcConfig"
       OR content MATCHES "FcPattern" OR content MATCHES "FcFontList")
        message(FATAL_ERROR "fontconfig reference outside the discovery provider in ${source}")
    endif()
endforeach()

file(
    GLOB_RECURSE discovery_sources
    LIST_DIRECTORIES false
    "${fontconfig_root}/*.h"
    "${fontconfig_root}/*.cpp"
)
foreach(source IN LISTS discovery_sources)
    file(READ "${source}" content)
    if(content MATCHES "<QtDBus/" OR content MATCHES "<QtQml/" OR content MATCHES "<QtQuick/"
       OR content MATCHES "<QtWidgets/" OR content MATCHES "QGuiApplication"
       OR content MATCHES "<QtCore/QProcess")
        message(FATAL_ERROR "Forbidden font-discovery dependency in ${source}")
    endif()
endforeach()

message(STATUS "Font discovery is the sole fontconfig boundary and stays transport-free")
