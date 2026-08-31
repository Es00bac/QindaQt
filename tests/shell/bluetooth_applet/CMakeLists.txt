# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtBluetoothAppletTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS
                 Core DBus Gui Qml Quick QuickControls2 Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory("../../../src/services/bluetooth_protocol"
                     "${CMAKE_BINARY_DIR}/src/services/bluetooth_protocol")
    add_subdirectory("../../../src/services/bluetooth_client"
                     "${CMAKE_BINARY_DIR}/src/services/bluetooth_client")
    add_subdirectory("../../../src/shell/bluetooth_applet"
                     "${CMAKE_BINARY_DIR}/src/shell/bluetooth_applet")
    set(bluetooth_applet_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
else()
    set(bluetooth_applet_source_root "${CMAKE_SOURCE_DIR}")
endif()

qt_add_executable(qindaqt_bluetooth_applet_presentation_tests
                  tst_bluetooth_applet_presentation.cpp)
target_link_libraries(
    qindaqt_bluetooth_applet_presentation_tests
    PRIVATE QindaQt::ShellBluetoothApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_applet_presentation_tests)
add_test(
    NAME qindaqt.bluetooth-applet-presentation
    COMMAND qindaqt_bluetooth_applet_presentation_tests
)
set_tests_properties(
    qindaqt.bluetooth-applet-presentation PROPERTIES LABELS "shell;bluetooth;pure"
)

qt_add_executable(qindaqt_bluetooth_applet_request_tests
                  tst_bluetooth_request_state.cpp)
target_link_libraries(
    qindaqt_bluetooth_applet_request_tests
    PRIVATE QindaQt::ShellBluetoothApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_applet_request_tests)
add_test(
    NAME qindaqt.bluetooth-applet-request-state
    COMMAND qindaqt_bluetooth_applet_request_tests
)
set_tests_properties(
    qindaqt.bluetooth-applet-request-state PROPERTIES LABELS "shell;bluetooth;pure"
)

qt_add_executable(qindaqt_bluetooth_applet_controller_tests
                  tst_bluetooth_applet_controller.cpp)
target_include_directories(
    qindaqt_bluetooth_applet_controller_tests
    PRIVATE "${bluetooth_applet_source_root}/tests/services/bluetooth_client"
)
target_link_libraries(
    qindaqt_bluetooth_applet_controller_tests
    PRIVATE QindaQt::ShellBluetoothAppletRuntime Qt6::Test
)
qindaqt_enable_warnings(qindaqt_bluetooth_applet_controller_tests)
add_test(
    NAME qindaqt.bluetooth-applet-controller
    COMMAND qindaqt_bluetooth_applet_controller_tests
)
set_tests_properties(
    qindaqt.bluetooth-applet-controller PROPERTIES LABELS "shell;bluetooth;runtime"
)

qt_add_executable(qindaqt_bluetooth_applet_qml_tests
                  tst_bluetooth_applet_qml.cpp)
target_include_directories(
    qindaqt_bluetooth_applet_qml_tests
    PRIVATE "${bluetooth_applet_source_root}/tests/services/bluetooth_client"
)
target_link_libraries(
    qindaqt_bluetooth_applet_qml_tests
    PRIVATE
        QindaQt::ShellBluetoothAppletRuntime
        qindaqt_shell_bluetooth_applet_runtimeplugin
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_bluetooth_applet_qml_tests
    PRIVATE
        QINDAQT_BLUETOOTH_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
qindaqt_enable_warnings(qindaqt_bluetooth_applet_qml_tests)
qt_import_qml_plugins(qindaqt_bluetooth_applet_qml_tests)
add_test(
    NAME qindaqt.bluetooth-applet-offscreen
    COMMAND qindaqt_bluetooth_applet_qml_tests
)
set_tests_properties(
    qindaqt.bluetooth-applet-offscreen
    PROPERTIES
        LABELS "shell;bluetooth;runtime;offscreen;accessibility"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

add_test(
    NAME qindaqt.bluetooth-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${bluetooth_applet_source_root}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.bluetooth-applet-boundary
    PROPERTIES LABELS "shell;bluetooth;pure;policy;hostile"
)

add_test(
    NAME qindaqt.bluetooth-applet-runtime-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${bluetooth_applet_source_root}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/runtime-boundary-poison"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_runtime_boundary.cmake"
)
set_tests_properties(
    qindaqt.bluetooth-applet-runtime-boundary
    PROPERTIES LABELS "shell;bluetooth;runtime;policy;hostile"
)

if(TARGET qindaqt-shell)
    add_test(
        NAME qindaqt.bluetooth-applet-installed-package
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
            "-DQINDAQT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DQINDAQT_STAGE=${CMAKE_CURRENT_BINARY_DIR}/installed-bluetooth-applet"
            "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_bluetooth_applet.cmake"
    )
    set_tests_properties(
        qindaqt.bluetooth-applet-installed-package
        PROPERTIES LABELS "shell;bluetooth;runtime;installed;poison" TIMEOUT 60
    )
endif()
