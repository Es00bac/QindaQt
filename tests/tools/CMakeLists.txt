# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME qindaqt.dev-harness
    COMMAND
        "${Python3_EXECUTABLE}"
        -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}"
        -p "test_*.py"
)

set_tests_properties(
    qindaqt.dev-harness
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/tools"
)

# ADR-0286: every bundled wallpaper is a named, credited landscape image of at
# least the minimum width. Reads PNG headers only; no image library needed.
add_test(
    NAME qindaqt.wallpapers-bundle
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/import-wallpapers" --check
)
set_tests_properties(qindaqt.wallpapers-bundle PROPERTIES LABELS "unit;tools;wallpaper")

add_test(
    NAME qindaqt.agent-input-installed
    COMMAND
        "${CMAKE_COMMAND}"
        "-DAGENT_INPUT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
        "-DAGENT_INPUT_INSTALL_ROOT=${CMAKE_CURRENT_BINARY_DIR}/agent-input-install"
        "-DAGENT_INPUT_BINDIR=${CMAKE_INSTALL_BINDIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_agent_input_smoke.cmake"
)
set_tests_properties(
    qindaqt.agent-input-installed
    PROPERTIES LABELS "unit;install;tools;portal"
)
