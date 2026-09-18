# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing About Computer boundary poison input: ${required}")
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
        message(FATAL_ERROR "About Computer Settings accepted poison ${poison_name}")
    endif()
    string(CONCAT log "${output}" "${error}")
    if(NOT log MATCHES "${expected}")
        message(FATAL_ERROR "About Computer poison failed for wrong reason:\n${log}")
    endif()
endfunction()

file(REMOVE_RECURSE "${POISON_ROOT}")
file(MAKE_DIRECTORY "${POISON_ROOT}")
file(WRITE "${POISON_ROOT}/allowed.cpp"
    "#include <qindaqt/apps/settings_about_computer/about_computer_info.h>\n#include <QtCore/QObject>\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSCAN_ROOT=${POISON_ROOT}"
                -P "${CHECK_SCRIPT}" RESULT_VARIABLE allowed_status
                OUTPUT_VARIABLE allowed_output ERROR_VARIABLE allowed_error)
if(NOT allowed_status EQUAL 0)
    message(FATAL_ERROR
        "About Computer Settings rejected its positive allow-list control:\n${allowed_output}${allowed_error}")
endif()

require_rejected(sibling "poison.cpp"
    "#include \"src/apps/settings_center/settings_route_registry.h\""
    "non-public repository header")
require_rejected(escape "poison.cpp"
    "#include \"../settings_center/settings_route_registry.h\""
    "non-public repository header")
require_rejected(dbus_leak "poison.cpp"
    "#include <QtDBus/QDBusConnection>"
    "leaked D-Bus outside")
require_rejected(subprocess "poison.cpp"
    "void poison() { QProcess p; p.start(QStringLiteral(\"equery\"), {}); }"
    "shelled out to a subprocess")
file(REMOVE_RECURSE "${POISON_ROOT}")
message(STATUS "About Computer Settings rejected all independent boundary poisons")
