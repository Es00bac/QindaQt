# SPDX-License-Identifier: GPL-3.0-or-later

# Static boundary gate for the Audio applet pure projection. Runs without
# configure: cmake -DSOURCE_ROOT=<repo> -P check_boundary.cmake
#
# The pure projection is bounded presentation over public Audio1 protocol
# values. It must never link or include transport, QML, platform, or service
# implementation surfaces.

set(allowed_include_roots
    "qindaqt/services/audio_protocol/"
)

set(forbidden_substrings
    "QtDBus"
    "QtQml"
    "QtQuick"
    "QtGui"
    "QtWidgets"
    "QTimer"
    "QFile"
    "QSocket"
    "QProcess"
    "QNetwork"
    "QDBus"
    "wireplumber"
    "WirePlumber"
    "pipewire"
    "PipeWire"
    "glib"
    "GLib"
)

# Production presentation values must not derive QObject either; the QtTest
# harness in tests legitimately needs a QObject-derived test class.
set(production_extra_forbidden
    "QObject"
)

set(sources
    "${SOURCE_ROOT}/src/shell/audio_applet/audio_applet_model.h"
    "${SOURCE_ROOT}/src/shell/audio_applet/audio_applet_model.cpp"
    "${SOURCE_ROOT}/tests/shell/audio_applet/tst_audio_applet_model.cpp"
)
list(LENGTH sources source_count)
if(source_count EQUAL 0)
    message(FATAL_ERROR "Audio applet boundary gate found no sources")
endif()

set(violations "")
foreach(path IN LISTS sources)
    file(READ "${path}" content)
    foreach(forbidden IN LISTS forbidden_substrings)
        string(FIND "${content}" "${forbidden}" hit)
        if(NOT hit EQUAL -1)
            list(APPEND violations "${path}: forbidden token '${forbidden}'")
        endif()
    endforeach()
    string(FIND "${path}" "/src/shell/audio_applet/" production_path)
    if(NOT production_path EQUAL -1)
        foreach(forbidden IN LISTS production_extra_forbidden)
            string(FIND "${content}" "${forbidden}" hit)
            if(NOT hit EQUAL -1)
                list(APPEND violations "${path}: forbidden token '${forbidden}'")
            endif()
        endforeach()
    endif()
    string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]([^\">]+)" includes "${content}")
    foreach(include_line IN LISTS includes)
        string(REGEX REPLACE "#[ \t]*include[ \t]*[<\"]" "" include_path "${include_line}")
        set(allowed FALSE)
        foreach(root IN LISTS allowed_include_roots)
            string(FIND "${include_path}" "${root}" position)
            if(position EQUAL 0)
                set(allowed TRUE)
                break()
            endif()
        endforeach()
        if(NOT allowed)
            string(FIND "${include_path}" "QtCore/" position)
            if(position EQUAL 0)
                set(allowed TRUE)
            endif()
        endif()
        if(NOT allowed)
            string(FIND "${include_path}" "QtTest" position)
            if(position EQUAL 0)
                set(allowed TRUE)
            endif()
        endif()
        if(NOT allowed)
            # Slash-less includes are the C++ standard library and local test
            # fixtures; anything namespaced must match the boundary roots.
            string(FIND "${include_path}" "/" position)
            if(position EQUAL -1)
                set(allowed TRUE)
            endif()
        endif()
        if(NOT allowed)
            list(APPEND violations "${path}: non-boundary include '${include_path}'")
        endif()
    endforeach()
endforeach()

if(violations)
    foreach(violation IN LISTS violations)
        message(SEND_ERROR "${violation}")
    endforeach()
    message(FATAL_ERROR "Audio applet boundary gate failed")
endif()

message(STATUS "Audio applet boundary gate passed (${source_count} files)")
