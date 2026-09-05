# SPDX-License-Identifier: GPL-3.0-or-later

# Shell iconography focused tests. Fixture trees are generated at test time
# beneath the test binary's build directory, never under /tmp or the source
# tree. Every row strips the host display and session-bus endpoints; GUI rows
# force the offscreen software backend.

find_package(Qt6 6.11 REQUIRED COMPONENTS Gui Qml Quick Test)

function(qindaqt_add_shell_icons_test target source test_name)
    qt_add_executable(${target} ${source} shell_icons_test_fixtures.h)
    target_link_libraries(${target} PRIVATE QindaQt::ShellIcons Qt6::Gui Qt6::Test)
    target_compile_definitions(
        ${target}
        PRIVATE
            QINDAQT_SHELL_ICONS_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/fixtures"
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(
        ${test_name}
        PROPERTIES
            LABELS "shell;icons"
            ENVIRONMENT
                "DISPLAY=;WAYLAND_DISPLAY=;DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    )
endfunction()

qindaqt_add_shell_icons_test(
    qindaqt_shell_icons_locator_tests
    tst_shell_icons_locator.cpp
    qindaqt.shell-icons-locator
)
qindaqt_add_shell_icons_test(
    qindaqt_shell_icons_resolver_tests
    tst_shell_icons_resolver.cpp
    qindaqt.shell-icons-resolver
)

# Provider rows render offscreen under fatal warnings.
qindaqt_add_shell_icons_test(
    qindaqt_shell_icons_provider_tests
    tst_shell_icons_provider.cpp
    qindaqt.shell-icons-provider
)
target_link_libraries(
    qindaqt_shell_icons_provider_tests
    PRIVATE
        Qt6::Qml
        Qt6::Quick
)
set_tests_properties(
    qindaqt.shell-icons-provider
    PROPERTIES
        LABELS "shell;icons;offscreen"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;DISPLAY=;WAYLAND_DISPLAY=;DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
)

# The compiled-module row wires the production IconRuntime seam into a real
# engine, publishes a QST-1 theme through the Tokens plugin path, and
# instantiates the compiled Icon element.
qt_add_executable(
    qindaqt_shell_icons_qml_tests
    tst_shell_icons_qml.cpp
    shell_icons_test_fixtures.h
)
target_link_libraries(
    qindaqt_shell_icons_qml_tests
    PRIVATE
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        QindaQt::TokensQml
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_shell_icons_qml_tests
    PRIVATE
        QINDAQT_SHELL_ICONS_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/fixtures"
        QINDAQT_SHELL_ICONS_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_shell_icons_qml_tests)
qt_import_qml_plugins(qindaqt_shell_icons_qml_tests)
add_dependencies(
    qindaqt_shell_icons_qml_tests
    qindaqt_tokens_qmlplugin
)
add_test(NAME qindaqt.shell-icons-qml-offscreen COMMAND qindaqt_shell_icons_qml_tests)
# AGENT-GUARD: QT_FATAL_WARNINGS=1 is load-bearing — the compiled module must
# load and render warning-free offscreen, including the placeholder path.
set_tests_properties(
    qindaqt.shell-icons-qml-offscreen
    PROPERTIES
        LABELS "shell;icons;offscreen"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml;DISPLAY=;WAYLAND_DISPLAY=;DBUS_SESSION_BUS_ADDRESS=;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
        TIMEOUT 300
)
