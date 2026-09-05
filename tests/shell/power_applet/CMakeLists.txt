# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtPowerAppletTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS
                 Core DBus Gui Qml Quick QuickControls2 Svg Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory("../../../src/services/power_protocol"
                     "${CMAKE_BINARY_DIR}/src/services/power_protocol")
    add_subdirectory("../../../src/services/brightness_model"
                     "${CMAKE_BINARY_DIR}/src/services/brightness_model")
    add_subdirectory("../../../src/services/power_client"
                     "${CMAKE_BINARY_DIR}/src/services/power_client")
    add_subdirectory("../../../src/shell/power_applet"
                     "${CMAKE_BINARY_DIR}/src/shell/power_applet")
    set(power_applet_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
else()
    find_package(Qt6 6.11 REQUIRED COMPONENTS Svg)
    set(power_applet_source_root "${CMAKE_SOURCE_DIR}")
endif()

qt_add_executable(qindaqt_power_applet_presentation_tests
                  tst_power_applet_presentation.cpp)
target_link_libraries(
    qindaqt_power_applet_presentation_tests
    PRIVATE QindaQt::ShellPowerApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_applet_presentation_tests)
add_test(
    NAME qindaqt.power-applet-presentation
    COMMAND qindaqt_power_applet_presentation_tests
)
set_tests_properties(
    qindaqt.power-applet-presentation PROPERTIES LABELS "shell;power;pure"
)

qt_add_executable(qindaqt_power_applet_controls_tests
                  tst_power_applet_controls.cpp)
target_link_libraries(
    qindaqt_power_applet_controls_tests
    PRIVATE QindaQt::ShellPowerApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_applet_controls_tests)
add_test(
    NAME qindaqt.power-applet-control-rows
    COMMAND qindaqt_power_applet_controls_tests
)
set_tests_properties(
    qindaqt.power-applet-control-rows PROPERTIES LABELS "shell;power;pure"
)

qt_add_executable(qindaqt_power_applet_request_tests
                  tst_brightness_request_state.cpp)
target_link_libraries(
    qindaqt_power_applet_request_tests
    PRIVATE QindaQt::ShellPowerApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_applet_request_tests)
add_test(
    NAME qindaqt.power-applet-request-state
    COMMAND qindaqt_power_applet_request_tests
)
set_tests_properties(
    qindaqt.power-applet-request-state PROPERTIES LABELS "shell;power;pure"
)

qt_add_executable(qindaqt_power_applet_controller_tests
                  tst_power_applet_controller.cpp)
target_include_directories(
    qindaqt_power_applet_controller_tests
    PRIVATE "${power_applet_source_root}/tests/services/power_client"
)
target_link_libraries(
    qindaqt_power_applet_controller_tests
    PRIVATE QindaQt::ShellPowerAppletRuntime Qt6::Test
)
qindaqt_enable_warnings(qindaqt_power_applet_controller_tests)
add_test(
    NAME qindaqt.power-applet-controller
    COMMAND qindaqt_power_applet_controller_tests
)
set_tests_properties(
    qindaqt.power-applet-controller PROPERTIES LABELS "shell;power;runtime"
)

qt_add_executable(qindaqt_power_applet_qml_tests tst_power_applet_qml.cpp)
target_include_directories(
    qindaqt_power_applet_qml_tests
    PRIVATE "${power_applet_source_root}/tests/services/power_client"
)
target_link_libraries(
    qindaqt_power_applet_qml_tests
    PRIVATE
        QindaQt::ShellPowerAppletRuntime
        qindaqt_shell_power_applet_runtimeplugin
        QindaQt::TokensQml
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_power_applet_qml_tests
    PRIVATE
        QINDAQT_POWER_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
add_dependencies(qindaqt_power_applet_qml_tests qindaqt_tokens_qmlplugin)
qindaqt_enable_warnings(qindaqt_power_applet_qml_tests)
qt_import_qml_plugins(qindaqt_power_applet_qml_tests)
add_test(
    NAME qindaqt.power-applet-offscreen
    COMMAND qindaqt_power_applet_qml_tests
)
set_tests_properties(
    qindaqt.power-applet-offscreen
    PROPERTIES
        LABELS "shell;power;runtime;offscreen;accessibility"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
)

add_test(
    NAME qindaqt.power-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${power_applet_source_root}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)

add_test(
    NAME qindaqt.power-applet-runtime-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${power_applet_source_root}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/runtime-boundary-poison"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_runtime_boundary.cmake"
)
set_tests_properties(
    qindaqt.power-applet-runtime-boundary
    PROPERTIES LABELS "shell;power;runtime;policy;hostile"
)

if(TARGET qindaqt-shell)
    add_test(
        NAME qindaqt.power-applet-installed-package
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
            "-DQINDAQT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DQINDAQT_STAGE=${CMAKE_CURRENT_BINARY_DIR}/installed-power-applet"
            "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_power_applet.cmake"
    )
    set_tests_properties(
        qindaqt.power-applet-installed-package
        PROPERTIES LABELS "shell;power;runtime;installed;poison" TIMEOUT 60
    )
endif()
set_tests_properties(
    qindaqt.power-applet-boundary PROPERTIES LABELS "shell;power;pure;policy"
)
