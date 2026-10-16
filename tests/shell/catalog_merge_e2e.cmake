# SPDX-License-Identifier: GPL-3.0-or-later
#
# Included from tests/shell/CMakeLists.txt inside its if(TARGET qindaqt-shell) block
# so the large parent stays within the source-shape policy.

# ADR-0393 end to end: the real binary, a stale user theme directory.
add_test(
    NAME qindaqt.shell-catalog-merge
    COMMAND "${CMAKE_COMMAND}"
        "-DSHELL_EXECUTABLE=$<TARGET_FILE:qindaqt-shell>"
        "-DSANDBOX_ROOT=${CMAKE_CURRENT_BINARY_DIR}/shell-catalog-merge"
        "-DSOURCE_THEME_DIR=${PROJECT_SOURCE_DIR}/data/themes"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_shell_catalog_merge.cmake"
)
set_tests_properties(qindaqt.shell-catalog-merge PROPERTIES
    LABELS "shell;catalog;e2e" TIMEOUT 120)
