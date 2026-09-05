# SPDX-License-Identifier: GPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtShellTaskListTests LANGUAGES CXX)

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
    include(QindaQtCompiler)
    find_package(ECM 6.0 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
    include(KDEInstallDirs6)
    # The applet subdirectory of src/shell/task_list and the offscreen QML
    # rows below need the GUI stack even in a standalone configure.
    find_package(Qt6 6.11 REQUIRED COMPONENTS
                 Core DBus Gui Qml Quick QuickControls2 QuickTest Test)
    qt_standard_project_setup(REQUIRES 6.11)

    add_subdirectory(
        "../../../src/shell_visibility_protocol"
        "${CMAKE_BINARY_DIR}/src/shell_visibility_protocol"
    )
    # The T2 applet QML imports QindaQt.Controls/QindaQt.Tokens, so the
    # standalone route must provide the themes/tokens/controls targets before
    # the task-list module (the repository build reaches them through src/).
    add_subdirectory(
        "../../../src/themes"
        "${CMAKE_BINARY_DIR}/src/themes"
    )
    add_subdirectory(
        "../../../src/design_tokens"
        "${CMAKE_BINARY_DIR}/src/design_tokens"
    )
    add_subdirectory(
        "../../../src/controls"
        "${CMAKE_BINARY_DIR}/src/controls"
    )
    add_subdirectory(
        "../../../src/shell/task_list"
        "${CMAKE_BINARY_DIR}/src/shell/task_list"
    )
endif()

function(qindaqt_add_task_list_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE QindaQt::ShellTaskList Qt6::Test)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "shell;task-list;pure")
endfunction()

function(qindaqt_add_task_list_transport_test target source test_name)
    # Extra headers with Q_OBJECT fakes must be listed as target sources so
    # AUTOMOC mocs them; a shared fake header cannot use the <basename>.moc
    # include pattern from an unrelated test source.
    qt_add_executable(${target} ${source} ${ARGN})
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::ShellTaskList
            QindaQt::ShellTaskListProducer
            QindaQt::ShellTaskListOperations
            Qt6::DBus
            Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

qindaqt_add_task_list_test(
    qindaqt_task_list_values_tests
    tst_task_list_values.cpp
    qindaqt.task-list-values
)
qindaqt_add_task_list_test(
    qindaqt_task_list_source_grouping_tests
    tst_task_list_source_grouping.cpp
    qindaqt.task-list-source-grouping
)
qindaqt_add_task_list_test(
    qindaqt_task_list_source_validation_tests
    tst_task_list_source_validation.cpp
    qindaqt.task-list-source-validation
)
qindaqt_add_task_list_test(
    qindaqt_task_list_intents_tests
    tst_task_list_intents.cpp
    qindaqt.task-list-intents
)
qindaqt_add_task_list_test(
    qindaqt_task_list_scope_filter_tests
    tst_task_list_scope_filter.cpp
    qindaqt.task-list-scope-filter
)
qindaqt_add_task_list_test(
    qindaqt_task_list_presentation_tests
    tst_task_list_presentation.cpp
    qindaqt.task-list-presentation
)

qindaqt_add_task_list_transport_test(
    qindaqt_task_list_wire_tests
    tst_task_list_wire.cpp
    qindaqt.task-list-wire
)
set_tests_properties(
    qindaqt.task-list-wire PROPERTIES LABELS "shell;task-list;pure"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_facts_producer_tests
    tst_task_list_facts_producer.cpp
    qindaqt.task-list-facts-producer
)
set_tests_properties(
    qindaqt.task-list-facts-producer PROPERTIES LABELS "shell;task-list"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_operation_adapter_tests
    tst_task_list_operation_adapter.cpp
    qindaqt.task-list-operation-adapter
    task_list_operation_test_support.h
)
set_tests_properties(
    qindaqt.task-list-operation-adapter PROPERTIES LABELS "shell;task-list"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_operation_results_tests
    tst_task_list_operation_results.cpp
    qindaqt.task-list-operation-results
    task_list_operation_test_support.h
)
set_tests_properties(
    qindaqt.task-list-operation-results PROPERTIES LABELS "shell;task-list"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_operation_lineage_tests
    tst_task_list_operation_lineage.cpp
    qindaqt.task-list-operation-lineage
    task_list_operation_test_support.h
)
set_tests_properties(
    qindaqt.task-list-operation-lineage PROPERTIES LABELS "shell;task-list"
)
qindaqt_add_task_list_transport_test(
    qindaqt_task_list_qt_transports_tests
    tst_task_list_qt_transports.cpp
    qindaqt.task-list-qt-transports
)
set_tests_properties(
    qindaqt.task-list-qt-transports PROPERTIES LABELS "shell;task-list;dbus"
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(task_list_boundary_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
else()
    set(task_list_boundary_source_root "${CMAKE_SOURCE_DIR}")
endif()
add_test(
    NAME qindaqt.task-list-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${task_list_boundary_source_root}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_task_list_boundary.cmake"
)
set_tests_properties(qindaqt.task-list-boundary PROPERTIES
    LABELS "shell;task-list;pure;policy"
)

# --- Task-list T2 applet (src/shell/task_list/applet) ---

function(qindaqt_add_task_list_applet_test target source test_name labels)
    # Q_OBJECT fake headers must be listed as target sources so AUTOMOC mocs
    # them (task_list_operation_test_support.h precedent above).
    qt_add_executable(${target} ${source} ${ARGN})
    target_link_libraries(
        ${target}
        PRIVATE
            QindaQt::ShellTaskListApplet
            QindaQt::ShellTaskListProducer
            QindaQt::ShellTaskListOperations
            Qt6::Test
    )
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
endfunction()

qindaqt_add_task_list_applet_test(
    qindaqt_task_list_applet_projection_tests
    tst_task_list_applet_projection.cpp
    qindaqt.task-list-applet-projection
    "shell;task-list;pure"
)
qindaqt_add_task_list_applet_test(
    qindaqt_task_list_applet_controller_tests
    tst_task_list_applet_controller.cpp
    qindaqt.task-list-applet-controller
    "shell;task-list"
    task_list_applet_test_fakes.h
    task_list_operation_test_support.h
)
qindaqt_add_task_list_applet_test(
    qindaqt_task_list_applet_operations_tests
    tst_task_list_applet_operations.cpp
    qindaqt.task-list-applet-operation-dispatch
    "shell;task-list"
    task_list_operation_test_support.h
)

# Offscreen QML rows: one harness executable driving the REAL controller with
# fakes against the compiled QindaQt.Shell.TaskList module; two ctest rows
# select the phase-rendering and keyboard slots by QTest argv filter. The
# applet QML consumes QST-1 roles, so the harness publishes a real theme
# through the staged Tokens/Controls plugin path before any row (audio applet
# precedent); both plugin targets are build prerequisites of the executable.
qt_add_executable(
    qindaqt_task_list_applet_qml_tests
    tst_task_list_applet_qml.cpp
    task_list_applet_test_fakes.h
    task_list_operation_test_support.h
)
target_link_libraries(
    qindaqt_task_list_applet_qml_tests
    PRIVATE
        QindaQt::ShellTaskListApplet
        QindaQt::TokensQml
        qindaqt_shell_task_list_appletplugin
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_task_list_applet_qml_tests
    PRIVATE
        QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${task_list_boundary_source_root}"
)
qindaqt_enable_warnings(qindaqt_task_list_applet_qml_tests)
qt_import_qml_plugins(qindaqt_task_list_applet_qml_tests)
add_dependencies(
    qindaqt_task_list_applet_qml_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
)

# AGENT-GUARD: QT_FATAL_WARNINGS=1 is load-bearing — the compiled module must
# render warning-free offscreen. Do not drop it to silence a Qt warning; fix
# the cause or report it.
function(qindaqt_add_task_list_applet_qml_test test_name test_slot)
    add_test(
        NAME ${test_name}
        COMMAND qindaqt_task_list_applet_qml_tests ${test_slot}
    )
    set_tests_properties(
        ${test_name}
        PROPERTIES
            LABELS "shell;task-list;offscreen"
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
            TIMEOUT 300
    )
endfunction()

qindaqt_add_task_list_applet_qml_test(
    qindaqt.task-list-applet-qml-offscreen
    phasesRenderWithTruthfulObjectNames
)
qindaqt_add_task_list_applet_qml_test(
    qindaqt.task-list-applet-qml-arrows-offscreen
    arrowTraversalStopsAtStripEndpoints
)
qindaqt_add_task_list_applet_qml_test(
    qindaqt.task-list-applet-qml-keyboard-offscreen
    keyboardTraversalAndContextMenuDispatch
)

add_test(
    NAME qindaqt.task-list-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${task_list_boundary_source_root}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/task-list-applet-boundary-poison"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_task_list_applet_boundary.cmake"
)
set_tests_properties(qindaqt.task-list-applet-boundary PROPERTIES
    LABELS "shell;task-list;policy"
)

# Installed component consumer/probe: validates the TaskListAppletRuntime
# install rules end-to-end (public boundary, archives, compiled QML module,
# manifest, shell binary) from a fresh staged prefix, including the
# relocated-consumer RPATH truth. Sibling T0/T1 task-list artifacts are staged
# by the harness because their install components belong to their owners.
if(TARGET qindaqt-shell)
    add_test(
        NAME qindaqt.task-list-applet-installed-package
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
            "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DQINDAQT_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}"
            "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-task-list-applet-stage"
            "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
            "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
            "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
            "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
            "-DQINDAQT_CONFIGURATION=$<CONFIG>"
            "-DQINDAQT_TASK_LIST_LIBRARY=$<TARGET_FILE:qindaqt_shell_task_list>"
            "-DQINDAQT_TASK_LIST_PRODUCER_LIBRARY=$<TARGET_FILE:qindaqt_shell_task_list_producer>"
            "-DQINDAQT_TASK_LIST_OPERATIONS_LIBRARY=$<TARGET_FILE:qindaqt_shell_task_list_operations>"
            "-DQINDAQT_COMPOSITOR_SHELL_ACTIONS_LIBRARY=$<TARGET_FILE:qindaqt_compositor_shell_actions>"
            "-DQINDAQT_TASK_LIST_INCLUDE_DIR=${task_list_boundary_source_root}/src/shell/task_list/include"
            "-DQINDAQT_TASK_LIST_PRODUCER_INCLUDE_DIR=${task_list_boundary_source_root}/src/shell/task_list/producer/include"
            "-DQINDAQT_TASK_LIST_OPERATIONS_INCLUDE_DIR=${task_list_boundary_source_root}/src/shell/task_list/operations/include"
            "-DQINDAQT_CONTROLS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Controls"
            "-DQINDAQT_TOKENS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens"
            "-DQINDAQT_CONTROLS_LIBRARY=$<TARGET_FILE:qindaqt_controls_qml>"
            "-DQINDAQT_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_tokens_qml>"
            "-DQINDAQT_THEMES_LIBRARY=$<TARGET_FILE:qindaqt_themes>"
            "-DQINDAQT_DESIGN_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_design_tokens>"
            "-DQINDAQT_THEMES_INCLUDE_DIR=${task_list_boundary_source_root}/src/themes/include"
            "-DQINDAQT_DESIGN_TOKENS_INCLUDE_DIR=${task_list_boundary_source_root}/src/design_tokens/include"
            "-DQINDAQT_THEME_FILE=${task_list_boundary_source_root}/data/themes/qinda-light.json"
            "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_task_list_consumer"
            "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_task_list_cpp_consumer.cpp"
            "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
            "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
            "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DQINDAQT_STATIC_LIBRARY_SUFFIX=${CMAKE_STATIC_LIBRARY_SUFFIX}"
            "-DQINDAQT_SHARED_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_task_list_applet.cmake"
    )
    set_tests_properties(
        qindaqt.task-list-applet-installed-package
        PROPERTIES LABELS "shell;task-list;installed;poison" RUN_SERIAL TRUE TIMEOUT 300
    )

    find_program(QINDAQT_TASK_LIST_DBUS_RUN_SESSION dbus-run-session REQUIRED)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime")
    qt_add_executable(
        qindaqt_task_list_applet_composition_private_bus_tests
        tst_task_list_applet_composition_private_bus.cpp
        task_list_applet_test_fakes.h
        task_list_operation_test_support.h
        "${task_list_boundary_source_root}/src/shell/runtime/tasklistappletcomposition.cpp"
        "${task_list_boundary_source_root}/src/shell/runtime/tasklistappletcomposition.h"
    )
    target_include_directories(
        qindaqt_task_list_applet_composition_private_bus_tests
        PRIVATE "${task_list_boundary_source_root}/src/shell/runtime"
    )
    target_link_libraries(
        qindaqt_task_list_applet_composition_private_bus_tests
        PRIVATE
            QindaQt::AppletHost
            QindaQt::AppletRuntime
            QindaQt::ShellTaskListApplet
            QindaQt::ShellTaskListProducer
            QindaQt::ShellTaskListOperations
            QindaQt::ShellWindowActionsClient
            Qt6::DBus
            Qt6::Test
    )
    target_compile_definitions(
        qindaqt_task_list_applet_composition_private_bus_tests
        PRIVATE QINDAQT_SOURCE_DIR="${task_list_boundary_source_root}"
    )
    qindaqt_enable_warnings(
        qindaqt_task_list_applet_composition_private_bus_tests)
    add_test(
        NAME qindaqt.task-list-applet-composition-private-bus
        COMMAND "${QINDAQT_TASK_LIST_DBUS_RUN_SESSION}" --
                $<TARGET_FILE:qindaqt_task_list_applet_composition_private_bus_tests>
    )
    set_tests_properties(
        qindaqt.task-list-applet-composition-private-bus
        PROPERTIES
            LABELS "shell;task-list;runtime;private-bus;hostile"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
            TIMEOUT 30
    )

    add_test(
        NAME qindaqt.task-list-applet-panel-dispatcher
        COMMAND
            $<TARGET_FILE:Qt6::qmltestrunner>
            -import "${task_list_boundary_source_root}/tests/shell/qml/imports"
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_task_list_dispatcher.qml"
    )
    set_tests_properties(
        qindaqt.task-list-applet-panel-dispatcher
        PROPERTIES
            LABELS "shell;task-list;runtime;offscreen;accessibility"
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
            ENVIRONMENT_MODIFICATION
                "DBUS_SESSION_BUS_ADDRESS=unset:;DISPLAY=unset:;WAYLAND_DISPLAY=unset:;XDG_RUNTIME_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/isolated-runtime"
            TIMEOUT 30
    )
endif()
