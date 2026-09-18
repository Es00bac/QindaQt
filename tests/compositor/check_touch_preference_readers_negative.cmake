# SPDX-License-Identifier: GPL-3.0-or-later
# The reader scan must reject (a) a page key the compositor never reads and
# (b) a decoded field the plugin never applies: the exact defect classes of
# the O14 remainder's first cut.

foreach(required IN ITEMS SOURCE_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing touch reader poison input: ${required}")
    endif()
endforeach()

function(stage_tree)
    file(REMOVE_RECURSE "${POISON_ROOT}")
    foreach(relative IN ITEMS
            src/apps/settings/input/touch_settings_model.cpp
            src/compositor/kwin/touchedgeactions.cpp
            src/compositor/kwin/qindaqtkwinplugin.cpp)
        get_filename_component(parent "${POISON_ROOT}/${relative}" DIRECTORY)
        file(MAKE_DIRECTORY "${parent}")
        file(COPY "${SOURCE_ROOT}/${relative}" DESTINATION "${parent}")
    endforeach()
endfunction()

function(expect_rejected poison_name expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSCAN_ROOT=${POISON_ROOT}"
                    -P "${CHECK_SCRIPT}" RESULT_VARIABLE status
                    OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(status EQUAL 0)
        message(FATAL_ERROR "touch reader scan accepted poison ${poison_name}")
    endif()
    if(NOT "${output}${error}" MATCHES "${expected}")
        message(FATAL_ERROR "touch reader poison ${poison_name} failed for the wrong reason:\n${output}${error}")
    endif()
endfunction()

# Control: the unmodified tree passes.
stage_tree()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSCAN_ROOT=${POISON_ROOT}" -P "${CHECK_SCRIPT}"
                RESULT_VARIABLE control_status OUTPUT_VARIABLE control_output
                ERROR_VARIABLE control_error)
if(NOT control_status EQUAL 0)
    message(FATAL_ERROR "touch reader scan rejected the unmodified tree:\n${control_output}${control_error}")
endif()

# (a) the page scopes a key nobody decodes
stage_tree()
file(APPEND "${POISON_ROOT}/src/apps/settings/input/touch_settings_model.cpp"
     "\nstatic const char *poisonKey = \"input.touch.hapticStrength\";\n")
expect_rejected(unread_key "hapticStrength")

# (b) a field is decoded and then ignored: the remainder's own defect
stage_tree()
file(READ "${POISON_ROOT}/src/compositor/kwin/qindaqtkwinplugin.cpp" plugin_text)
# The reference stays in a comment: prose must not count as a reader.
string(REPLACE "applyTouchscreenEnabled(preferences.touchscreenEnabled);"
       "// poison: applyTouchscreenEnabled(preferences.touchscreenEnabled) never called"
       plugin_text "${plugin_text}")
file(WRITE "${POISON_ROOT}/src/compositor/kwin/qindaqtkwinplugin.cpp" "${plugin_text}")
expect_rejected(unapplied_field "touchscreenEnabled is decoded")

file(REMOVE_RECURSE "${POISON_ROOT}")
message(STATUS "touch reader scan rejected both poisons")
