# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Clipboard boundary poison input: ${required}")
    endif()
endforeach()

function(require_rejected poison_name relative_path source_text expected)
    file(REMOVE_RECURSE "${POISON_ROOT}")
    get_filename_component(parent "${POISON_ROOT}/${relative_path}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent}")
    file(WRITE "${POISON_ROOT}/${relative_path}" "${source_text}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSCAN_ROOT=${POISON_ROOT}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(status EQUAL 0)
        message(FATAL_ERROR "Clipboard Settings accepted poison ${poison_name}")
    endif()
    string(CONCAT log "${output}" "${error}")
    if(NOT log MATCHES "${expected}")
        message(FATAL_ERROR "Clipboard poison failed for wrong reason:\n${log}")
    endif()
endfunction()

file(REMOVE_RECURSE "${POISON_ROOT}")
file(MAKE_DIRECTORY "${POISON_ROOT}")
file(WRITE "${POISON_ROOT}/allowed.cpp"
    "#include <qindaqt/services/clipboard_client/clipboard_client.h>\n#include <qindaqt/services/settings_client/settings_client.h>\n#include <QtCore/QObject>\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSCAN_ROOT=${POISON_ROOT}"
                -P "${CHECK_SCRIPT}" RESULT_VARIABLE allowed_status
                OUTPUT_VARIABLE allowed_output ERROR_VARIABLE allowed_error)
if(NOT allowed_status EQUAL 0)
    message(FATAL_ERROR
        "Clipboard Settings rejected its positive allow-list control:\n${allowed_output}${allowed_error}")
endif()

require_rejected(sibling "poison.cpp"
    "#include \"src/apps/settings_center/settings_route_registry.h\""
    "non-public repository header")
require_rejected(escape "poison.cpp"
    "#include \"../settings_center/settings_route_registry.h\""
    "non-public repository header")
require_rejected(private_service "poison.cpp"
    "#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>"
    "public-client-only boundary")
require_rejected(payload_decode "poison.cpp"
    "void poison() { decodeValue({}); }"
    "forbidden content-read")
require_rejected(copy_intent "poison.h"
    "class Poison { Q_INVOKABLE bool copyEntry(); };"
    "forbidden content-read")
require_rejected(qml_read "Poison.qml"
    "Item { Component.onCompleted: clipboardSettings.readEntry() }"
    "forbidden content authority")
require_rejected(dbus_leak "poison.cpp"
    "#include <QtDBus/QDBusConnection>"
    "D-Bus outside")
file(REMOVE_RECURSE "${POISON_ROOT}")
message(STATUS "Clipboard Settings rejected all independent boundary poisons")
