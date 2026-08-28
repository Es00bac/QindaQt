# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED QINDAQT_CONTROLS_SOURCE_DIR)
    message(FATAL_ERROR "QINDAQT_CONTROLS_SOURCE_DIR is required")
endif()

file(GLOB qml_files "${QINDAQT_CONTROLS_SOURCE_DIR}/*.qml")
list(LENGTH qml_files qml_count)
if(NOT qml_count EQUAL 14)
    message(FATAL_ERROR "Expected 14 public Controls QML files, found ${qml_count}")
endif()

set(forbidden_imports
    "QindaQt.Settings"
    "QindaQt.Shell"
    "QindaQt.AppShell"
    "LayerShellQt"
    "org.kde.kirigami"
)

foreach(qml IN LISTS qml_files)
    file(READ "${qml}" content)
    if(content MATCHES "#[0-9A-Fa-f]+")
        message(FATAL_ERROR "Palette hex literal is forbidden in ${qml}")
    endif()
    if(content MATCHES "qinda-(light|dusk|dark|high-contrast|macos)"
       OR content MATCHES "sourceThemeId")
        message(FATAL_ERROR "Theme identity branch is forbidden in ${qml}")
    endif()
    foreach(forbidden IN LISTS forbidden_imports)
        if(content MATCHES "import[ \t]+${forbidden}")
            message(FATAL_ERROR "Forbidden dependency ${forbidden} in ${qml}")
        endif()
    endforeach()
endforeach()

# The visual-row wrapper must reject ambiguous selectors before it can invoke
# a binary. The 25 registered runtime rows then prove that each valid selector
# yields exactly one tagged QtTest pass.
set(visual_runner "${CMAKE_CURRENT_LIST_DIR}/run_controls_visual_row.cmake")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -DQINDAQT_VISUAL_TEST=/nonexistent-controls-visual-test
        -DQINDAQT_SCALE=100
        -P "${visual_runner}"
    RESULT_VARIABLE missing_row_result
    OUTPUT_QUIET ERROR_QUIET
)
if(missing_row_result EQUAL 0)
    message(FATAL_ERROR "visual-row runner accepted a missing row selector")
endif()
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -DQINDAQT_VISUAL_TEST=/nonexistent-controls-visual-test
        -DQINDAQT_SCALE=100
        -DQINDAQT_ROW=qinda-unknown-compact
        -P "${visual_runner}"
    RESULT_VARIABLE unknown_row_result
    OUTPUT_QUIET ERROR_QUIET
)
if(unknown_row_result EQUAL 0)
    message(FATAL_ERROR "visual-row runner accepted an unknown row selector")
endif()

message(STATUS "Validated ${qml_count} Controls QML files: token-only, no theme IDs or hex palette literals; visual selector rejects missing/unknown rows")
