# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Windows boundary poison input: ${required}")
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
        message(FATAL_ERROR "Windows Settings accepted poison ${poison_name}")
    endif()
    string(CONCAT log "${output}" "${error}")
    if(NOT log MATCHES "${expected}")
        message(FATAL_ERROR "Windows poison failed for wrong reason:\n${log}")
    endif()
endfunction()

file(REMOVE_RECURSE "${POISON_ROOT}")
file(MAKE_DIRECTORY "${POISON_ROOT}")
file(WRITE "${POISON_ROOT}/allowed.cpp"
    "#include <qindaqt/services/settings_client/settings_client.h>\n#include <qindaqt/apps/settings_windows/windows_values.h>\n#include <QtCore/QObject>\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSCAN_ROOT=${POISON_ROOT}"
                -P "${CHECK_SCRIPT}" RESULT_VARIABLE allowed_status
                OUTPUT_VARIABLE allowed_output ERROR_VARIABLE allowed_error)
if(NOT allowed_status EQUAL 0)
    message(FATAL_ERROR
        "Windows Settings rejected its positive allow-list control:\n${allowed_output}${allowed_error}")
endif()

require_rejected(sibling "poison.cpp"
    "#include \"src/apps/settings_center/settings_route_registry.h\""
    "non-public repository header")
require_rejected(escape "poison.cpp"
    "#include \"../settings_center/settings_route_registry.h\""
    "non-public repository header")
require_rejected(consumer_reach "poison.cpp"
    "#include <qindaqt/platform/qt_theme/qindaqt_platform_theme.h>"
    "public-client-only boundary")
require_rejected(token_deriver "poison.cpp"
    "#include <qindaqt/design_tokens/accessibility_inputs.h>"
    "public-client-only boundary")
require_rejected(reserved_key "poison.cpp"
    "constexpr char Poison[] = \"windowManagement.sessionRestore\";"
    "reserved session-restore key")
require_rejected(qml_reserved_key "Poison.qml"
    "Item { property string key: \"windowManagement.sessionRestore\" }"
    "reserved session-restore key")
require_rejected(bridge_reach "poison.cpp"
    "#include <qindaqt/session/window_management/window_management_bridge.h>"
    "public-client-only boundary")
require_rejected(kconfig_reach "poison.cpp"
    "#include <KConfigGroup>"
    "public-client-only boundary")
require_rejected(dbus_leak "poison.cpp"
    "#include <QtDBus/QDBusConnection>"
    "D-Bus into the route")
file(REMOVE_RECURSE "${POISON_ROOT}")
message(STATUS "Windows Settings rejected all independent boundary poisons")
