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

# Mutation-sensitive negative controls independently prove that the exact
# include allowlist rejects transport, QML, and service-internal reach. The
# fixture stays under the caller-supplied build-tree root (POISON_ROOT).
set(poison_cases "")
if(DEFINED POISON_ROOT AND NOT AUDIO_APPLET_PURE_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")

    function(expect_pure_poison_rejected name relative_path poison_content)
        set(case_root "${poison_root}/${name}")
        file(MAKE_DIRECTORY "${case_root}")
        foreach(source IN LISTS sources)
            file(RELATIVE_PATH relative_source "${SOURCE_ROOT}" "${source}")
            get_filename_component(source_dir "${case_root}/${relative_source}" DIRECTORY)
            file(MAKE_DIRECTORY "${source_dir}")
            file(COPY "${source}" DESTINATION "${source_dir}")
        endforeach()
        file(APPEND "${case_root}/${relative_path}" "${poison_content}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_ROOT=${case_root}"
                    -DAUDIO_APPLET_PURE_POLICY_SKIP_POISON=ON
                    -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE poison_status
            OUTPUT_VARIABLE poison_output
            ERROR_VARIABLE poison_error)
        if(poison_status EQUAL 0)
            message(FATAL_ERROR
                "Audio applet boundary accepted ${name} poison:\n"
                "${poison_output}${poison_error}")
        endif()
        set(poison_cases ${poison_cases} "${name}" PARENT_SCOPE)
    endfunction()

    expect_pure_poison_rejected(
        "transport" "src/shell/audio_applet/audio_applet_model.h"
        "#include <QtDBus/QDBusConnection>\n")
    expect_pure_poison_rejected(
        "service-internal" "src/shell/audio_applet/audio_applet_model.cpp"
        "#include <qindaqt/services/audio_service/resident_audio_service.h>\n")
    expect_pure_poison_rejected(
        "qml" "src/shell/audio_applet/audio_applet_model.h"
        "#include <QtQml/QQmlEngine>\n")
    expect_pure_poison_rejected(
        "qobject" "src/shell/audio_applet/audio_applet_model.h"
        "class QObject;\n")
    list(LENGTH poison_cases poison_rejections)
    if(NOT poison_rejections EQUAL 4)
        message(FATAL_ERROR
            "Audio applet boundary expected 4 poison rejections, got ${poison_rejections}")
    endif()
    file(REMOVE_RECURSE "${poison_root}")
else()
    set(poison_rejections 0)
endif()

message(STATUS
    "Audio applet boundary gate passed (${source_count} files and ${poison_rejections} poison rejections)")
