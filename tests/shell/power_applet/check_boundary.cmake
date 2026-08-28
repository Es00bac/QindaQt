# SPDX-License-Identifier: LGPL-3.0-or-later

# Static boundary gate for the Power applet presentation model. Runs without
# configure: cmake -DSOURCE_ROOT=<repo> -P check_boundary.cmake
#
# The module is pure presentation over public PB-0 values. It must never link
# or include transport, QML, platform, or service implementation surfaces.

set(allowed_include_roots
    "qindaqt/shell/power_applet/"
    "qindaqt/services/power_protocol/"
    "qindaqt/services/brightness_model/"
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
    "sysfs"
    "upower"
    "UPower"
    "logind"
)

# Production presentation values must not derive QObject either; the QtTest
# harness in tests legitimately needs a QObject-derived test class.
set(production_extra_forbidden
    "QObject"
)

file(
    GLOB_RECURSE sources
    "${SOURCE_ROOT}/src/shell/power_applet/*.h"
    "${SOURCE_ROOT}/src/shell/power_applet/*.cpp"
    "${SOURCE_ROOT}/tests/shell/power_applet/*.h"
    "${SOURCE_ROOT}/tests/shell/power_applet/*.cpp"
)
list(LENGTH sources source_count)
if(source_count EQUAL 0)
    message(FATAL_ERROR "Power applet boundary gate found no sources")
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
    string(FIND "${path}" "/src/shell/power_applet/" production_path)
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
    message(FATAL_ERROR "Power applet boundary gate failed")
endif()

message(STATUS "Power applet boundary gate passed (${source_count} files)")
