# SPDX-License-Identifier: GPL-3.0-or-later
#
# End-to-end proof of ADR-0393 against the real shell binary: `qindaqt-shell
# --list` loads its catalogs exactly as a session does (ShellRuntimeApplication::
# loadCatalogs), so a stale user theme directory must neither hide the other
# themes nor stop the shell, and a hand-edited bad file must be reported and
# skipped. The user directory below plays the part of the September
# ~/.local/share/qindaqt/themes that rejected the saved theme at every login.

foreach(required IN ITEMS SHELL_EXECUTABLE SANDBOX_ROOT SOURCE_THEME_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing shell catalog input: ${required}")
    endif()
endforeach()
if(NOT EXISTS "${SHELL_EXECUTABLE}")
    message(FATAL_ERROR "shell executable does not exist: ${SHELL_EXECUTABLE}")
endif()

file(REMOVE_RECURSE "${SANDBOX_ROOT}")
file(MAKE_DIRECTORY "${SANDBOX_ROOT}/home/qindaqt/themes" "${SANDBOX_ROOT}/system"
                    "${SANDBOX_ROOT}/runtime")
file(CHMOD "${SANDBOX_ROOT}/runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

# A stale copy of one shipped theme under the same id, and an unusable file.
file(READ "${SOURCE_THEME_DIR}/qinda-dark.json" stale_theme)
string(REPLACE "\"name\": \"QindaQt Smoked Plum\"" "\"name\": \"STALE Smoked Plum\""
       stale_theme "${stale_theme}")
if(NOT stale_theme MATCHES "STALE Smoked Plum")
    message(FATAL_ERROR "could not derive the stale theme from qinda-dark.json")
endif()
file(WRITE "${SANDBOX_ROOT}/home/qindaqt/themes/qinda-dark.json" "${stale_theme}")
file(WRITE "${SANDBOX_ROOT}/home/qindaqt/themes/broken.json" "{ this is not json")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        --unset=DISPLAY
        --unset=WAYLAND_DISPLAY
        --unset=QML_IMPORT_PATH
        --unset=QML2_IMPORT_PATH
        QT_QPA_PLATFORM=offscreen
        QML_DISABLE_DISK_CACHE=1
        DBUS_SESSION_BUS_ADDRESS=unix:path=${SANDBOX_ROOT}/absent-session-bus
        XDG_DATA_HOME=${SANDBOX_ROOT}/home
        XDG_DATA_DIRS=${SANDBOX_ROOT}/system
        XDG_CONFIG_HOME=${SANDBOX_ROOT}/config
        XDG_CACHE_HOME=${SANDBOX_ROOT}/cache
        XDG_RUNTIME_DIR=${SANDBOX_ROOT}/runtime
        "${SHELL_EXECUTABLE}" --list
    WORKING_DIRECTORY "${SANDBOX_ROOT}"
    TIMEOUT 90
    RESULT_VARIABLE status
    OUTPUT_VARIABLE listing
    ERROR_VARIABLE diagnostics
)
if(NOT "${status}" STREQUAL "0")
    message(FATAL_ERROR
        "qindaqt-shell --list failed (${status}):\n${listing}\n${diagnostics}")
endif()

# The installed set is intact: the theme named in the incident is offered.
foreach(theme IN ITEMS qinda-corner-violet-dark qinda-light qinda-high-contrast)
    if(NOT listing MATCHES "  ${theme} - ")
        message(FATAL_ERROR "theme ${theme} is missing from the merged catalog:\n${listing}")
    endif()
endforeach()
# The user layer overrides by id, and only the id it names.
if(NOT listing MATCHES "  qinda-dark - STALE Smoked Plum")
    message(FATAL_ERROR "the user override of qinda-dark did not win:\n${listing}")
endif()
if(listing MATCHES "STALE Smoked Plum[^\n]*\n[^\n]*STALE Smoked Plum")
    message(FATAL_ERROR "the user override appears more than once:\n${listing}")
endif()
# The bad override file is reported and skipped, not fatal.
if(NOT diagnostics MATCHES "ignored an invalid override catalog entry:[^\n]*broken\\.json")
    message(FATAL_ERROR "the invalid override was not reported:\n${diagnostics}")
endif()
# The layering is logged, user directory last, so a shadowing directory is
# visible in the session log instead of silent.
if(NOT diagnostics MATCHES "catalog directories, low to high precedence:[^\n]*home/qindaqt/themes")
    message(FATAL_ERROR "the catalog directories were not logged:\n${diagnostics}")
endif()
message(STATUS "qindaqt-shell merged the stale user theme directory over the installed themes")
