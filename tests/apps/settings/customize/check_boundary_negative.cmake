# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing boundary poison input: ${required}")
    endif()
endforeach()

function(require_rejected poison_name hostile_include)
    file(REMOVE_RECURSE "${POISON_ROOT}")
    file(MAKE_DIRECTORY "${POISON_ROOT}")
    file(WRITE "${POISON_ROOT}/${poison_name}.cpp"
         "#include \"${hostile_include}\"\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSCAN_ROOT=${POISON_ROOT}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE poison_status
        OUTPUT_VARIABLE poison_output
        ERROR_VARIABLE poison_error
    )
    if(poison_status EQUAL 0)
        message(FATAL_ERROR
            "Customize ${poison_name} poison was accepted:\n${poison_output}${poison_error}")
    endif()
    set(poison_log "${poison_output}${poison_error}")
    string(FIND "${poison_log}" "'${hostile_include}'" rejection_position)
    if(rejection_position EQUAL -1)
        message(FATAL_ERROR
            "Customize ${poison_name} failed for the wrong reason:\n${poison_log}")
    endif()
endfunction()

# AGENT-GUARD: Keep both independent. The sibling path catches narrow lists of
# known-private modules; the escape catches relative paths around any such list.
require_rejected(
    sibling_repository_header
    "src/apps/settings_center/settings_route_registry.h")
require_rejected(parent_directory_escape "../settings_center/settings_route_registry.h")
file(REMOVE_RECURSE "${POISON_ROOT}")
