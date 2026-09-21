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

# The controller: the overview's only mutable state, and its only authority
# decision. Qt Core, so no display.
qt_add_executable(
    qindaqt_gather_overview_controller_tests tst_gather_overview_controller.cpp
)
target_link_libraries(
    qindaqt_gather_overview_controller_tests
    PRIVATE QindaQt::ShellGatherOverview Qt6::Test
)
set_target_properties(
    qindaqt_gather_overview_controller_tests
    PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_gather_overview_controller_tests)
add_test(
    NAME qindaqt.gather-overview-controller
    COMMAND qindaqt_gather_overview_controller_tests
)
set_tests_properties(
    qindaqt.gather-overview-controller
    PROPERTIES TIMEOUT 30 LABELS "unit;pure;shell"
)

# The drawing surface over a stub projection. The surface QML resolves QST-1
# roles from the Tokens singleton in the same engine, so the harness publishes
# a real theme before creating anything (task-list applet precedent); both the
# Tokens and Controls plugins are build prerequisites.
# The grab row writes a reviewable PNG here; the arrangement is a visual
# design and an assertion cannot say whether it looks right.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
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
        QINDAQT_GATHER_OVERVIEW_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/artifacts"
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

# The composition: the shell's owner of the overview, over the REAL task-list
# applet controller rather than a stub, because the whole claim of this layer
# is that the overview borrows that controller's grants and its stale-revision
# arbitration instead of carrying any of its own. start() is never called, so
# no output surface and no compositor are needed.
#
# AGENT-CONTRACT: this target must not be built where the production shell is
# off - the composition lives in src/shell/runtime and links LayerShellQt.
if(QINDAQT_BUILD_PRODUCTION_SHELL)
    # src/shell/CMakeLists.txt finds this inside its own production-shell
    # branch, so the imported target is not visible from this directory.
    find_package(LayerShellQt 6.6.6 REQUIRED)
    qt_add_executable(
        qindaqt_gather_overview_composition_tests
        tst_gather_overview_composition.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/runtime/gatheroverviewcomposition.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/runtime/gatheroverviewcomposition.h"
        # AGENT-GUARD: both fakes are header-only QObjects, so AUTOMOC only
        # generates their vtables if the headers are listed as sources. Without
        # these two lines the link fails on "vtable for FakeOperationAuthority"
        # - the same precedent tests/shell/task_list/CMakeLists.txt records.
        "${CMAKE_CURRENT_LIST_DIR}/../../shell/task_list/task_list_applet_test_fakes.h"
        "${CMAKE_CURRENT_LIST_DIR}/../../shell/task_list/task_list_operation_test_support.h"
    )
    target_include_directories(
        qindaqt_gather_overview_composition_tests
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/../../../src/shell/runtime"
            "${CMAKE_CURRENT_LIST_DIR}/../../shell/task_list"
    )
    target_link_libraries(
        qindaqt_gather_overview_composition_tests
        PRIVATE
            QindaQt::ShellGatherOverview
            QindaQt::ShellTaskListApplet
            QindaQt::ShellTaskListProducer
            QindaQt::ShellTaskListOperations
            LayerShellQt::Interface
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
    )
    set_target_properties(
        qindaqt_gather_overview_composition_tests
        PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
    )
    qindaqt_enable_warnings(qindaqt_gather_overview_composition_tests)
    add_test(
        NAME qindaqt.gather-overview-composition
        COMMAND qindaqt_gather_overview_composition_tests
    )
    set_tests_properties(
        qindaqt.gather-overview-composition
        PROPERTIES
            TIMEOUT 60
            LABELS "shell;gather;offscreen"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
