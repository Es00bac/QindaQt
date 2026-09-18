# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-NOTE: its own file because tests/session/CMakeLists.txt is at the
# 600-line shape limit. Harness-wide unit rows that guard the nested-session
# scaffolding itself, not one lane.

# AGENT-CONTRACT: nested drivers are exec'd by KWin (--exit-with-session); a
# driver committed 100644 hangs every fresh checkout's rows at their timeout
# while the author's chmod'd tree stays green. The row reads the git index.
add_test(
    NAME session.nested-driver-modes-unit
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_nested_driver_modes_unit.py"
)
set_tests_properties(session.nested-driver-modes-unit PROPERTIES
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS unit)
