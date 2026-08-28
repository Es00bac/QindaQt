# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT QINDAQT_SCALE MATCHES "^(100|125|150)$")
    message(FATAL_ERROR "QINDAQT_SCALE must be 100, 125, or 150")
endif()

set(themes
    qinda-light
    qinda-dusk
    qinda-dark
    qinda-high-contrast
    qinda-macos
)
set(rows)
if(QINDAQT_SCALE STREQUAL "100")
    foreach(theme IN LISTS themes)
        foreach(profile IN ITEMS compact ordinary large)
            list(APPEND rows "${theme}-${profile}")
        endforeach()
    endforeach()
else()
    set(rows ${themes})
endif()

if(NOT DEFINED QINDAQT_ROW OR QINDAQT_ROW STREQUAL "")
    message(FATAL_ERROR "QINDAQT_ROW must name exactly one visual data row")
endif()
list(FIND rows "${QINDAQT_ROW}" row_index)
if(row_index EQUAL -1)
    message(FATAL_ERROR
        "QINDAQT_ROW '${QINDAQT_ROW}' is not valid for scale ${QINDAQT_SCALE}"
    )
endif()
if(NOT DEFINED QINDAQT_VISUAL_TEST OR NOT EXISTS "${QINDAQT_VISUAL_TEST}")
    message(FATAL_ERROR "QINDAQT_VISUAL_TEST must name the built visual test executable")
endif()

# AGENT-GUARD: The Qt Quick software backend retains glyph/geometry render
# state across sequential QQuickWindow lifetimes in one QtTest process even
# when the QObject geometry is correct. Preserve one process per reviewed row;
# folding these invocations back into one data-driven process can silently
# write stale compact pixels while every logical assertion passes.
execute_process(
    COMMAND "${QINDAQT_VISUAL_TEST}" "matchesReviewedBaselines:${QINDAQT_ROW}" -v1
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Controls visual row ${QINDAQT_ROW} failed (exit ${result})\n${output}\n${error}"
    )
endif()

string(
    REGEX MATCHALL
    "PASS[ ]+: ControlsVisualTests::matchesReviewedBaselines\\([^)]*\\)"
    row_passes
    "${output}\n${error}"
)
list(LENGTH row_passes row_pass_count)
set(expected_pass
    "PASS   : ControlsVisualTests::matchesReviewedBaselines(${QINDAQT_ROW})"
)
list(FIND row_passes "${expected_pass}" expected_pass_index)
if(NOT row_pass_count EQUAL 1 OR expected_pass_index EQUAL -1)
    message(FATAL_ERROR
        "Visual invocation must pass exactly requested row ${QINDAQT_ROW}; "
        "observed ${row_pass_count} tagged row passes\n${output}\n${error}"
    )
endif()

message(STATUS "Controls visual row passed in isolation: ${QINDAQT_ROW}")
