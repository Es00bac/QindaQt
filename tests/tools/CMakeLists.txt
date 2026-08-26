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
