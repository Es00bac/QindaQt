# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellCustomizationEditorTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory("../../src/profiles" "${CMAKE_BINARY_DIR}/src/profiles")
    add_subdirectory("../../src/shell_layout" "${CMAKE_BINARY_DIR}/src/shell_layout")
    add_subdirectory("../../src/applets" "${CMAKE_BINARY_DIR}/src/applets")
    add_subdirectory(
        "../../src/shell_customization"
        "${CMAKE_BINARY_DIR}/src/shell_customization"
    )
    add_subdirectory(
        "../../src/shell_customization_editor"
        "${CMAKE_BINARY_DIR}/src/shell_customization_editor"
    )
endif()

function(qindaqt_add_shell_customization_editor_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(
        ${target}
        PRIVATE QindaQt::ShellCustomizationEditor Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_shell_customization_editor_test(
    qindaqt_customize_editor_intent_tests
    tst_intent_translation.cpp
    qindaqt.customize-editor-intent
)
qindaqt_add_shell_customization_editor_test(
    qindaqt_customize_editor_gesture_tests
    tst_gesture_state_machine.cpp
    qindaqt.customize-editor-gesture-machine
)
qindaqt_add_shell_customization_editor_test(
    qindaqt_customize_editor_session_tests
    tst_editor_session.cpp
    qindaqt.customize-editor-session
)
qindaqt_add_shell_customization_editor_test(
    qindaqt_customize_editor_persistence_tests
    tst_user_profile_store.cpp
    qindaqt.customize-editor-persistence
)
qindaqt_add_shell_customization_editor_test(
    qindaqt_customize_editor_accessibility_tests
    tst_accessibility_navigation.cpp
    qindaqt.customize-editor-accessibility
)
