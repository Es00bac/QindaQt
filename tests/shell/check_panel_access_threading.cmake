# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every applet-access object the runtime panel factory hands to
# createWithInitialProperties has to be declared on the panel root AND forwarded
# down the chain that reaches BuiltinAppletContent. Miss the declaration and Qt
# refuses the whole initial-property map with
#
#   Setting initial properties failed: RuntimePanel does not have a property
#   called obsAppletAccess
#
# once per panel, and every applet of that kind is created with a null access
# object. Miss the forwarding instead and there is no warning at all — the
# applet is simply dead. Both happened to the OBS applet between its lane
# landing and r9.

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(factory "${SOURCE_ROOT}/src/shell/runtime/runtimepanelwindowfactory.cpp")
file(READ "${factory}" factory_source)

# AGENT-GUARD: the keys are read out of the factory rather than restated here,
# so this check cannot drift from what is actually passed. If the map ever stops
# being a literal list of QStringLiteral keys, the count assertion below fails
# loudly instead of this check quietly matching nothing.
string(REGEX MATCHALL "QStringLiteral\\(\"[A-Za-z]+AppletAccess\"\\)"
       access_matches "${factory_source}")
set(access_keys "")
foreach(match IN LISTS access_matches)
    string(REGEX REPLACE "QStringLiteral\\(\"([A-Za-z]+)\"\\)" "\\1" key "${match}")
    list(APPEND access_keys "${key}")
endforeach()
list(REMOVE_DUPLICATES access_keys)
list(LENGTH access_keys access_count)
if(access_count LESS 10)
    message(FATAL_ERROR
        "expected at least 10 *AppletAccess keys in the panel factory, found "
        "${access_count} — the initial-property map is no longer a literal "
        "list of QStringLiteral keys and this check has gone blind")
endif()

# AGENT-GUARD: not every access object rides the initial-property map. Ones
# whose owner is built after the factory (desktopControlsAccess, and the gather
# overview's) are delivered with QObject::setProperty instead, which is
# deliberately forgiving - it is a silent no-op when the property is missing,
# which is what lets the C++ and QML halves land in either order. That
# forgiveness is exactly why they need this check more than the map keys do: a
# missing declaration produces NO warning at all for these, so the applet is
# simply dead with nothing in the log. Read them out of the factory too.
string(REGEX MATCHALL "setProperty\\\(\"[A-Za-z]+Access\"" set_matches
       "${factory_source}")
foreach(match IN LISTS set_matches)
    string(REGEX REPLACE "setProperty\\\(\"([A-Za-z]+)\"" "\\1" key "${match}")
    list(APPEND access_keys "${key}")
endforeach()
list(REMOVE_DUPLICATES access_keys)
list(LENGTH access_keys access_count)
if(access_count LESS 12)
    message(FATAL_ERROR
        "expected at least 12 applet-access keys in the panel factory, found "
        "${access_count} - the initial-property map or the setProperty calls "
        "are no longer literal and this check has gone blind")
endif()

# The chain the factory's map travels: declared at each step, forwarded to the
# next, and finally consumed by BuiltinAppletContent.
set(forwarding_files
    "${SOURCE_ROOT}/src/shell/qml/RuntimePanel.qml"
    "${SOURCE_ROOT}/src/shell/qml/PanelContent.qml"
    "${SOURCE_ROOT}/src/shell/qml/PanelAppletRow.qml"
    "${SOURCE_ROOT}/src/shell/qml/AppletChip.qml")
set(terminal_file "${SOURCE_ROOT}/src/shell/qml/BuiltinAppletContent.qml")

set(violations "")
foreach(key IN LISTS access_keys)
    foreach(path IN LISTS forwarding_files)
        file(READ "${path}" contents)
        if(NOT contents MATCHES "property var ${key}[ \t]*:")
            list(APPEND violations "${path}: no declaration of ${key}")
        endif()
        if(NOT contents MATCHES "${key}[ \t]*:[ \t]*root\\.${key}")
            list(APPEND violations "${path}: ${key} is declared but never forwarded")
        endif()
    endforeach()
    file(READ "${terminal_file}" terminal_contents)
    if(NOT terminal_contents MATCHES "property var ${key}[ \t]*:")
        list(APPEND violations "${terminal_file}: no declaration of ${key}")
    endif()
endforeach()

if(violations)
    string(REPLACE ";" "\n  " rendered "${violations}")
    message(FATAL_ERROR
        "runtime panel applet-access threading is incomplete:\n  ${rendered}")
endif()

message(STATUS
    "runtime panel threads ${access_count} applet-access objects end to end")
