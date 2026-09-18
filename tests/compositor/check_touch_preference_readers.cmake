# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-CONTRACT (ADR-0205): every Settings1 key the Touch page writes must
# be decoded by the compositor's TouchPreferences, and every decoded field
# must be applied by the plugin. A key with no reader, or a field parsed and
# then ignored, is a control that lies (the O14 remainder shipped one);
# this scan fails on either. SCAN_ROOT overrides the source root for the
# negative (poison) test.

if(DEFINED SCAN_ROOT)
    set(root "${SCAN_ROOT}")
elseif(DEFINED SOURCE_ROOT)
    set(root "${SOURCE_ROOT}")
else()
    message(FATAL_ERROR "SOURCE_ROOT or SCAN_ROOT is required")
endif()

set(model "${root}/src/apps/settings/input/touch_settings_model.cpp")
set(decoder "${root}/src/compositor/kwin/touchedgeactions.cpp")
set(plugin "${root}/src/compositor/kwin/qindaqtkwinplugin.cpp")
foreach(required IN ITEMS model decoder plugin)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "touch preference reader scan: missing ${required} at ${${required}}")
    endif()
endforeach()
file(READ "${model}" model_text)
file(READ "${decoder}" decoder_text)
file(READ "${plugin}" plugin_text)
# A field named only in a comment is not read; scan code, not prose.
foreach(variable IN ITEMS model_text decoder_text plugin_text)
    string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" ${variable} "${${variable}}")
    string(REGEX REPLACE "//[^\n]*" "" ${variable} "${${variable}}")
endforeach()

# 1. Every key literal the page scopes ("input.touch.<name>") is read by the
#    decoder. Edge keys are built from a prefix on both sides, so the prefix
#    counts as the reader for them.
string(REGEX MATCHALL "\"input\\.touch\\.[A-Za-z]+\"" page_keys "${model_text}")
list(REMOVE_DUPLICATES page_keys)
list(LENGTH page_keys page_key_count)
if(page_key_count LESS 3)
    message(FATAL_ERROR "touch preference reader scan: found only ${page_key_count} page keys")
endif()
foreach(key IN LISTS page_keys)
    string(FIND "${decoder_text}" "${key}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "Touch page writes ${key} but the compositor never reads it (${decoder})")
    endif()
endforeach()

# 2. Every field the decoder assigns is consulted by the plugin's apply path
#    (outside the struct's own definition), so nothing is parsed into a field
#    nobody reads.
string(REGEX MATCHALL "preferences\\.([A-Za-z]+) *=" assignments "${decoder_text}")
set(fields)
foreach(assignment IN LISTS assignments)
    string(REGEX REPLACE "preferences\\.([A-Za-z]+) *=" "\\1" field "${assignment}")
    list(APPEND fields "${field}")
endforeach()
list(REMOVE_DUPLICATES fields)
list(LENGTH fields field_count)
if(field_count LESS 3)
    message(FATAL_ERROR "touch preference reader scan: found only ${field_count} decoded fields")
endif()
foreach(field IN LISTS fields)
    string(FIND "${plugin_text}" "preferences.${field}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "TouchPreferences::${field} is decoded but the plugin never applies it (${plugin})")
    endif()
endforeach()
message(STATUS "touch preference readers: ${page_key_count} page keys read, ${field_count} fields applied")
