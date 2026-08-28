# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 REQUIRED COMPONENTS QuickTest)

function(qindaqt_add_app_shell_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::AppShell Qt6::Test)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "app-shell")
endfunction()

qindaqt_add_app_shell_test(
    qindaqt_app_shell_action_registry_tests
    tst_action_registry.cpp
    qindaqt.app-shell-action-registry
)
set_tests_properties(
    qindaqt.app-shell-action-registry
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
qindaqt_add_app_shell_test(
    qindaqt_app_shell_coordinator_tests
    tst_application_coordinator.cpp
    qindaqt.app-shell-coordinator
)
set_tests_properties(
    qindaqt.app-shell-coordinator
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(qindaqt_app_shell_qml_tests tst_application_shell.cpp)
target_link_libraries(
    qindaqt_app_shell_qml_tests
    PRIVATE
        QindaQt::AppShell
        QindaQt::TokensQml
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_app_shell_qml_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_APP_SHELL_TEST_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
target_compile_features(qindaqt_app_shell_qml_tests PRIVATE cxx_std_20)
set_target_properties(qindaqt_app_shell_qml_tests PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_app_shell_qml_tests)
add_dependencies(
    qindaqt_app_shell_qml_tests
    qindaqt_app_shellplugin
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
)
add_test(NAME qindaqt.app-shell-surface-offscreen COMMAND qindaqt_app_shell_qml_tests)
set_tests_properties(
    qindaqt.app-shell-surface-offscreen
    PROPERTIES
        LABELS "app-shell;offscreen;accessibility"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

add_test(
    NAME qindaqt.app-shell-source-policy
    COMMAND
        "${CMAKE_COMMAND}"
        "-DAPP_SHELL_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/app_shell"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_app_shell_source_policy.cmake"
)
set_tests_properties(qindaqt.app-shell-source-policy PROPERTIES LABELS "app-shell;policy")

add_test(
    NAME qindaqt.app-shell-installed-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-app-shell-stage"
        "-DQINDAQT_APP_SHELL_SOURCE_DIRECTORY=${PROJECT_SOURCE_DIR}/src/app_shell"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
        "-DQINDAQT_APP_SHELL_LIBRARY=$<TARGET_FILE:qindaqt_app_shell>"
        "-DQINDAQT_APP_SHELL_PLUGIN=$<TARGET_FILE:qindaqt_app_shellplugin>"
        "-DQINDAQT_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_tokens_qml>"
        "-DQINDAQT_TOKENS_PLUGIN=$<TARGET_FILE:qindaqt_tokens_qmlplugin>"
        "-DQINDAQT_CONTROLS_LIBRARY=$<TARGET_FILE:qindaqt_controls_qml>"
        "-DQINDAQT_CONTROLS_PLUGIN=$<TARGET_FILE:qindaqt_controls_qmlplugin>"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_cpp_consumer.cpp"
        "-DQINDAQT_QML_TEST=${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_installed_app_shell.qml"
        "-DQINDAQT_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_app_shell_consumer.cmake"
)
set_tests_properties(
    qindaqt.app-shell-installed-consumer
    PROPERTIES LABELS "app-shell;package" RUN_SERIAL TRUE TIMEOUT 180
)
