# SPDX-License-Identifier: GPL-3.0-or-later

# The classification policy. Pure, so it needs no display and no fixtures
# beyond synthetic task-list projections.
qt_add_executable(
    qindaqt_gather_overview_projection_tests tst_gather_overview_projection.cpp
)
target_link_libraries(
    qindaqt_gather_overview_projection_tests
    PRIVATE QindaQt::ShellGatherOverview Qt6::Test
)
set_target_properties(
    qindaqt_gather_overview_projection_tests
    PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_gather_overview_projection_tests)
add_test(
    NAME qindaqt.gather-overview-projection
    COMMAND qindaqt_gather_overview_projection_tests
)
set_tests_properties(
    qindaqt.gather-overview-projection
    PROPERTIES TIMEOUT 20 LABELS "unit;pure;shell"
)

# The drawing surface over a stub projection. The surface QML resolves QST-1
# roles from the Tokens singleton in the same engine, so the harness publishes
# a real theme before creating anything (task-list applet precedent); both the
# Tokens and Controls plugins are build prerequisites.
qt_add_executable(
    qindaqt_gather_overview_surface_qml_tests tst_gather_overview_surface_qml.cpp
)
target_link_libraries(
    qindaqt_gather_overview_surface_qml_tests
    PRIVATE
        QindaQt::TokensQml
        qindaqt_shell_gather_overview_uiplugin
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_gather_overview_surface_qml_tests
    PRIVATE
        QINDAQT_GATHER_OVERVIEW_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
set_target_properties(
    qindaqt_gather_overview_surface_qml_tests
    PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_gather_overview_surface_qml_tests)
qt_import_qml_plugins(qindaqt_gather_overview_surface_qml_tests)
add_dependencies(
    qindaqt_gather_overview_surface_qml_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
)
add_test(
    NAME qindaqt.gather-overview-surface-qml
    COMMAND qindaqt_gather_overview_surface_qml_tests
)
set_tests_properties(
    qindaqt.gather-overview-surface-qml
    PROPERTIES
        TIMEOUT 120
        LABELS "shell;gather;offscreen;accessibility"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
