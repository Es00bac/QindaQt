# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellVisibilityTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory("../../src/profiles" "${CMAKE_BINARY_DIR}/src/profiles")
    add_subdirectory(
        "../../src/shell_visibility_protocol"
        "${CMAKE_BINARY_DIR}/src/shell_visibility_protocol"
    )
    add_subdirectory(
        "../../src/shell_visibility"
        "${CMAKE_BINARY_DIR}/src/shell_visibility"
    )
endif()

function(qindaqt_add_shell_visibility_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::ShellVisibility Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_shell_visibility_test(
    qindaqt_panel_visibility_modes_tests
    tst_panel_visibility_modes.cpp
    qindaqt.shell-visibility-modes
)
qindaqt_add_shell_visibility_test(
    qindaqt_panel_visibility_scope_tests
    tst_panel_visibility_scope.cpp
    qindaqt.shell-visibility-scope
)
qindaqt_add_shell_visibility_test(
    qindaqt_panel_visibility_validation_tests
    tst_panel_visibility_validation.cpp
    qindaqt.shell-visibility-validation
)
qindaqt_add_shell_visibility_test(
    qindaqt_compositor_visibility_snapshot_tests
    tst_compositor_visibility_snapshot.cpp
    qindaqt.shell-visibility-compositor-snapshot
)
qindaqt_add_shell_visibility_test(
    qindaqt_compositor_visibility_state_tests
    tst_compositor_visibility_state.cpp
    qindaqt.shell-visibility-compositor-state
)

if(TARGET QindaQt::CompositorBridge)
    qt_add_executable(
        qindaqt_compositor_visibility_wire_roundtrip_tests
        tst_compositor_visibility_wire_roundtrip.cpp
    )
    target_link_libraries(
        qindaqt_compositor_visibility_wire_roundtrip_tests
        PRIVATE
            QindaQt::CompositorBridge
            QindaQt::ShellVisibility
            Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_compositor_visibility_wire_roundtrip_tests)
    add_test(
        NAME qindaqt.shell-visibility-wire-roundtrip
        COMMAND qindaqt_compositor_visibility_wire_roundtrip_tests
    )
endif()
