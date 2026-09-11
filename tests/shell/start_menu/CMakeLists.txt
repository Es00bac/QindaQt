# SPDX-License-Identifier: GPL-3.0-or-later

# Start menu QML tests: the compiled QindaQt.Shell.StartMenu module offscreen
# over stub facades (button accessibility, fail-closed fallback, popup window
# contract, flat keyboard traversal, activation through the launcher facade,
# places opening through the places facade, log-off confirmation through the
# session-actions facade). Nothing here reaches the host session bus, a
# compositor, or a real process start.
#
# The block below lets this directory configure and run on its own for a
# focused lane build; the repository build wires it from tests/CMakeLists.txt.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellStartMenuTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    find_package(Qt6 6.11 REQUIRED COMPONENTS
                 Core Gui Qml Quick QuickControls2 Svg Test)
    qt_standard_project_setup(REQUIRES 6.11)

    # Tokens, Controls, and the shell icons module are the runtime imports of
    # the compiled StartMenu QML; the launcher pure model is icons' dependency.
    add_subdirectory("../../../src/themes" "${CMAKE_BINARY_DIR}/src/themes")
    add_subdirectory("../../../src/design_tokens"
                     "${CMAKE_BINARY_DIR}/src/design_tokens")
    add_subdirectory("../../../src/controls" "${CMAKE_BINARY_DIR}/src/controls")
    set(QINDAQT_LAUNCHER_PURE_ONLY ON)
    add_subdirectory("../../../src/shell/launcher"
                     "${CMAKE_BINARY_DIR}/src/shell/launcher")
    unset(QINDAQT_LAUNCHER_PURE_ONLY)
    add_subdirectory("../../../src/shell/icons"
                     "${CMAKE_BINARY_DIR}/src/shell/icons")
    add_subdirectory("../../../src/shell/start_menu"
                     "${CMAKE_BINARY_DIR}/src/shell/start_menu")
    set(start_menu_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
else()
    find_package(Qt6 6.11 REQUIRED COMPONENTS
                 Core Gui Qml Quick QuickControls2 Svg Test)
    set(start_menu_source_root "${CMAKE_SOURCE_DIR}")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

qt_add_executable(qindaqt_start_menu_qml_tests
    tst_start_menu_qml.cpp start_menu_qml_test_support.h)
target_link_libraries(
    qindaqt_start_menu_qml_tests
    PRIVATE
        QindaQt::TokensQml
        qindaqt_themes
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_start_menu_qml_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${start_menu_source_root}"
        QINDAQT_START_MENU_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
qindaqt_enable_warnings(qindaqt_start_menu_qml_tests)
qt_import_qml_plugins(qindaqt_start_menu_qml_tests)
# AGENT-CONTRACT: the applet imports QindaQt.Shell.StartMenu/Controls/Tokens
# through the build-tree QML directories, so the module plugins must exist
# before the offscreen scene loads.
add_dependencies(
    qindaqt_start_menu_qml_tests
    qindaqt_start_menu_qmlplugin
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
    qindaqt_shell_iconsplugin
)
add_test(NAME qindaqt.start-menu-qml COMMAND qindaqt_start_menu_qml_tests)
set_tests_properties(
    qindaqt.start-menu-qml
    PROPERTIES
        LABELS "shell;start-menu;offscreen;accessibility"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
        ENVIRONMENT_MODIFICATION
            "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
)
