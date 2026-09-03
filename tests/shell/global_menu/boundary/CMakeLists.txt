# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME qindaqt.global-menu-transport-boundary-poison
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/check_global_menu_transport_boundary.py"
        --source-root "${PROJECT_SOURCE_DIR}/src/shell/global_menu"
        --fixture-root "${CMAKE_CURRENT_BINARY_DIR}/poison-fixtures"
)

add_test(
    NAME qindaqt.global-menu-runtime-boundary-poison
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/check_global_menu_runtime_boundary.py"
        --runtime-source
            "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.cpp"
        --runtime-header
            "${PROJECT_SOURCE_DIR}/src/shell/runtime/globalmenuappletcomposition.h"
        --fixture-root "${CMAKE_CURRENT_BINARY_DIR}/runtime-poison-fixtures"
)
