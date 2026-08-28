# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 REQUIRED COMPONENTS Test)

# AGENT-CONTRACT: KDecoration3 publishes the relative plugin directory used by
# both the production install rule and this staged-install contract check.
find_package(KDecoration3 6.6 REQUIRED CONFIG)

foreach(test_name IN ITEMS installpaths kwincommandbuilder sessioncommandline sessiondefaults sessionenvironment)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_compile_features(tst_${test_name} PRIVATE cxx_std_20)
    target_link_libraries(tst_${test_name} PRIVATE QindaQt::SessionSupport Qt6::Test)
    set_target_properties(tst_${test_name} PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(tst_${test_name})
    add_test(NAME session.${test_name} COMMAND tst_${test_name})
endforeach()

qt_add_executable(
    qindaqt_shell_runtime_options_tests
    tst_runtimeoptions.cpp
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/runtimeoptions.cpp"
    "${PROJECT_SOURCE_DIR}/src/shell/runtime/runtimeoptions.h"
)
target_include_directories(
    qindaqt_shell_runtime_options_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/shell/runtime"
)
target_link_libraries(qindaqt_shell_runtime_options_tests PRIVATE Qt6::Core Qt6::Test)
set_target_properties(
    qindaqt_shell_runtime_options_tests PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_shell_runtime_options_tests)
add_test(
    NAME qindaqt.shell-runtime-options
    COMMAND qindaqt_shell_runtime_options_tests
)
set_tests_properties(
    qindaqt.shell-runtime-options PROPERTIES LABELS "unit;shell;session;security"
)

target_compile_definitions(
    tst_sessioncommandline
    PRIVATE QINDAQT_TEST_PRODUCTION_SHELL=$<BOOL:${QINDAQT_BUILD_PRODUCTION_SHELL}>
)

add_executable(
    tst_shellsurfaceprotocoltrace
    tst_shellsurfaceprotocoltrace.cpp
    shellsurfaceprotocolevidence.cpp
    shellsurfaceprotocoltrace.cpp
    shellsurfaceprotocoltrace.h
)
target_compile_features(tst_shellsurfaceprotocoltrace PRIVATE cxx_std_20)
target_link_libraries(tst_shellsurfaceprotocoltrace PRIVATE Qt6::Core Qt6::Test)
set_target_properties(tst_shellsurfaceprotocoltrace PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(tst_shellsurfaceprotocoltrace)
add_test(NAME shell.surface-protocol-trace COMMAND tst_shellsurfaceprotocoltrace)
set_tests_properties(shell.surface-protocol-trace PROPERTIES LABELS "unit;shell;wayland")

target_compile_definitions(
    tst_installpaths
    PRIVATE
        QINDAQT_EXPECTED_INSTALL_BIN_DIR="${KDE_INSTALL_BINDIR}"
        QINDAQT_EXPECTED_INSTALL_PLUGIN_DIR="${KDE_INSTALL_PLUGINDIR}"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME session.nested-scenario-parser
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session_unit.py"
)
add_test(
    NAME session.notification-live-driver-unit
    COMMAND
        "${CMAKE_COMMAND}" -E env "PYTHONDONTWRITEBYTECODE=1"
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_notification_live_unit.py"
)
set_tests_properties(
    session.nested-scenario-parser
    session.notification-live-driver-unit
    PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS unit
)

add_test(
    NAME session.python-syntax
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PYTHONPYCACHEPREFIX=${CMAKE_CURRENT_BINARY_DIR}/python-cache"
        "${Python3_EXECUTABLE}" -m py_compile
        "${CMAKE_CURRENT_SOURCE_DIR}/hybrid_pointer_validation.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/host_input_consent.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/nested_session_scenario.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/notification_live_arguments.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/notification_live_outer.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/notification_live_process.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/notification_live_restart.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/notification_live_stage.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_notification_live_nested.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_notification_live_unit.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/shell_surface_protocol_validation.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session_unit.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_parent_wayland_session.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_installed_plugin_discovery.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_plugin_unload.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_shell_surface_nested.py"
)
set_tests_properties(session.python-syntax PROPERTIES LABELS unit)

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session)
find_program(QINDAQT_BUSCTL busctl)
find_program(QINDAQT_KWIN_WAYLAND kwin_wayland)
find_program(QINDAQT_XDPYINFO xdpyinfo)
find_program(QINDAQT_XWAYLAND Xwayland)
find_program(QINDAQT_WESTON weston)
find_program(QINDAQT_DOTOOL dotool)

# AGENT-CONTRACT: qindaqt-wm defaults to its installed KDE plugin root. Every
# build-tree integration test must name the transient root explicitly.
set(_qindaqt_test_plugin_root "${CMAKE_BINARY_DIR}/plugins")
# AGENT-GUARD: Production shell qualification is also useful in an explicit
# bridge-only build. Keep its launcher root deterministic even when no QindaQt
# compositor plugin target populates this otherwise-empty staging directory.
file(MAKE_DIRECTORY "${_qindaqt_test_plugin_root}")

if(QINDAQT_DBUS_RUN_SESSION AND QINDAQT_KWIN_WAYLAND AND QINDAQT_XDPYINFO AND QINDAQT_XWAYLAND)
    qt_add_executable(
        qindaqt-session-probe
        compositordevelopmentworkflow.cpp
        compositordevelopmentworkflow.h
        compositoroutputworkflow.cpp
        compositoroutputworkflow.h
        compositorprobeclient.cpp
        compositorprobeclient.h
        compositorworkflow.cpp
        compositorworkflow.h
        hybridpointergeometry.cpp
        hybridpointergeometry.h
        hybridpointercontextmenu.cpp
        hybridpointercontextmenu.h
        hybridcompositorrestart.cpp
        hybridcompositorrestart.h
        hybridpointergrouping.cpp
        hybridpointergrouping.h
        hybridpointerinventory.cpp
        hybridpointerinventory.h
        hybridpointerraise.cpp
        hybridpointerraise.h
        hybridpointerworkflow.cpp
        hybridpointerworkflow.h
        hybridtestinputdriver.cpp
        hybridtestinputdriver.h
        sessionprobe.cpp
    )
    target_link_libraries(qindaqt-session-probe PRIVATE Qt6::Core Qt6::DBus Qt6::Gui)
    set_target_properties(qindaqt-session-probe PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt-session-probe)

    if(TARGET qindaqt-shell)
        qt_add_executable(
            qindaqt-shell-surface-session-probe
            shellsurfaceprobe.cpp
            shellsurfaceprotocolevidence.cpp
            shellsurfaceprotocoltrace.cpp
            shellsurfaceprotocoltrace.h
        )
        target_link_libraries(
            qindaqt-shell-surface-session-probe
            PRIVATE Qt6::Core Qt6::Gui
        )
        set_target_properties(
            qindaqt-shell-surface-session-probe
            PROPERTIES CXX_EXTENSIONS OFF
        )
        qindaqt_enable_warnings(qindaqt-shell-surface-session-probe)
        add_dependencies(
            qindaqt-shell-surface-session-probe
            qindaqt-shell
            qindaqt-wm
        )

        function(qindaqt_add_shell_surface_runtime_test test_name scenario_name)
            # AGENT-GUARD: The maximized probe intentionally conflicts with
            # QindaQt's intelligent shelf. Keep this initial-publication proof
            # on the never-hidden fixture; live hide transitions are separate.
            add_test(
                NAME "shell.production-surface.${test_name}"
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test_shell_surface_nested.py"
                    "$<TARGET_FILE:qindaqt-wm>"
                    "$<TARGET_FILE:qindaqt-shell-surface-session-probe>"
                    "$<TARGET_FILE:qindaqt-shell>"
                    "${QINDAQT_DBUS_RUN_SESSION}"
                    "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario_name}.json"
                    --plugin-root "${_qindaqt_test_plugin_root}"
                    --profile-dir
                    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/shell_surface_profiles"
                    --profile-id "qindaqt-surface-proof"
                    --theme-dir "${PROJECT_SOURCE_DIR}/data/themes"
            )
            set_tests_properties(
                "shell.production-surface.${test_name}"
                PROPERTIES
                    TIMEOUT 35
                    LABELS "integration;shell;wayland;layer-shell;display"
            )
        endfunction()

        qindaqt_add_shell_surface_runtime_test(1080p single-1080p)
        qindaqt_add_shell_surface_runtime_test(wuxga single-wuxga)
        qindaqt_add_shell_surface_runtime_test(1440p single-1440p)
    endif()

    add_test(
        NAME session.nested-wayland-xwayland
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session.py"
            "$<TARGET_FILE:qindaqt-wm>"
            "$<TARGET_FILE:qindaqt-session-probe>"
            "${QINDAQT_DBUS_RUN_SESSION}"
            "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
            --plugin-root "${_qindaqt_test_plugin_root}"
    )
    set_tests_properties(session.nested-wayland-xwayland PROPERTIES TIMEOUT 30 LABELS integration)

    if(QINDAQT_WESTON)
        add_test(
            NAME session.parent-wayland.weston-headless
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_parent_wayland_session.py"
                "$<TARGET_FILE:qindaqt-wm>"
                "$<TARGET_FILE:qindaqt-session-probe>"
                "${QINDAQT_DBUS_RUN_SESSION}"
                "${QINDAQT_WESTON}"
                "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                --plugin-root "${_qindaqt_test_plugin_root}"
        )
        set_tests_properties(
            session.parent-wayland.weston-headless
            PROPERTIES TIMEOUT 40 LABELS "integration;wayland;weston"
        )
    endif()

    if(TARGET qindaqt_compositor)
        include("${CMAKE_CURRENT_LIST_DIR}/NotificationLiveTests.cmake")

        qt_add_executable(
            qindaqt-plugin-unload-probe
            compositordevelopmentworkflow.h
            compositorprobeclient.cpp
            compositorprobeclient.h
            hybridpointergeometry.cpp
            hybridpointergeometry.h
            hybridpointergrouping.cpp
            hybridpointergrouping.h
            hybridpointerinventory.cpp
            hybridpointerinventory.h
            hybridpointertaskidentityworkflow.cpp
            hybridpointertaskidentityworkflow.h
            hybridpointerunloadworkflow.cpp
            hybridpointerunloadworkflow.h
            hybridtestinputdriver.cpp
            hybridtestinputdriver.h
            pluginunloadprobe.cpp
            pluginunloadworkflow.cpp
            pluginunloadworkflow.h
        )
        target_link_libraries(qindaqt-plugin-unload-probe PRIVATE Qt6::Core Qt6::DBus Qt6::Gui)
        set_target_properties(qindaqt-plugin-unload-probe PROPERTIES CXX_EXTENSIONS OFF)
        qindaqt_enable_warnings(qindaqt-plugin-unload-probe)

        function(qindaqt_add_virtual_output_test test_name scenario_name)
            add_test(
                NAME "session.virtual-output.${test_name}"
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session.py"
                    "$<TARGET_FILE:qindaqt-wm>"
                    "$<TARGET_FILE:qindaqt-session-probe>"
                    "${QINDAQT_DBUS_RUN_SESSION}"
                    "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario_name}.json"
                    --plugin-root "${_qindaqt_test_plugin_root}"
                    --inspect-compositor-outputs
            )
            set_tests_properties(
                "session.virtual-output.${test_name}"
                PROPERTIES TIMEOUT 30 LABELS "integration;display;compositor"
            )
        endfunction()

        # AGENT-CONTRACT: These names describe only the state the virtual CLI
        # applies: count plus a common mode/scale. Scenario topology and events
        # remain unapplied and are reported as such by the Python runner.
        qindaqt_add_virtual_output_test(single-1080p single-1080p)
        qindaqt_add_virtual_output_test(single-wuxga single-wuxga)
        qindaqt_add_virtual_output_test(single-1440p single-1440p)
        qindaqt_add_virtual_output_test(fractional-1080p-125 single-1080p-125)
        qindaqt_add_virtual_output_test(dual-common-1080p dual-1080p-horizontal)

        add_test(
            NAME compositor.kwin-plugin-nested
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session.py"
                "$<TARGET_FILE:qindaqt-wm>"
                "$<TARGET_FILE:qindaqt-session-probe>"
                "${QINDAQT_DBUS_RUN_SESSION}"
                "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                --plugin-root "${_qindaqt_test_plugin_root}"
                --expect-plugin
        )
        set_tests_properties(
            compositor.kwin-plugin-nested
            PROPERTIES TIMEOUT 30 LABELS "integration;compositor"
        )

        add_test(
            NAME compositor.kwin-plugin-unload-restores-clients
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_plugin_unload.py"
                "$<TARGET_FILE:qindaqt-wm>"
                "$<TARGET_FILE:qindaqt-plugin-unload-probe>"
                "${QINDAQT_DBUS_RUN_SESSION}"
                "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                --plugin-root "${_qindaqt_test_plugin_root}"
        )
        set_tests_properties(
            compositor.kwin-plugin-unload-restores-clients
            PROPERTIES TIMEOUT 30 LABELS "integration;compositor;lifecycle"
        )

        if(QINDAQT_DOTOOL AND QINDAQT_ENABLE_HOST_UINPUT_TESTS)
            add_test(
                NAME compositor.hybrid-pointer-plugin-unload-restores-clients
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test_plugin_unload.py"
                    "$<TARGET_FILE:qindaqt-wm>"
                    "$<TARGET_FILE:qindaqt-plugin-unload-probe>"
                    "${QINDAQT_DBUS_RUN_SESSION}"
                    "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                    --plugin-root "${_qindaqt_test_plugin_root}"
                    --expect-hybrid-pointer "${QINDAQT_DOTOOL}"
            )
            set_tests_properties(
                compositor.hybrid-pointer-plugin-unload-restores-clients
                PROPERTIES
                    # This lifecycle gate performs page activation, group
                    # minimize/restore, and bounded post-unload settlement in
                    # addition to the two real pointer transactions.
                    TIMEOUT 60
                    SKIP_RETURN_CODE 77
                    LABELS "integration;compositor;hybrid;input;uinput;lifecycle"
                    RUN_SERIAL TRUE
            )
        endif()

        add_test(
            NAME compositor.production-control-read-only
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session.py"
                "$<TARGET_FILE:qindaqt-wm>"
                "$<TARGET_FILE:qindaqt-session-probe>"
                "${QINDAQT_DBUS_RUN_SESSION}"
                "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                --plugin-root "${_qindaqt_test_plugin_root}"
                --expect-read-only
        )
        set_tests_properties(
            compositor.production-control-read-only
            PROPERTIES TIMEOUT 30 LABELS "integration;compositor;security"
        )

        if(QINDAQT_DOTOOL AND QINDAQT_ENABLE_HOST_UINPUT_TESTS)
            add_test(
                NAME compositor.hybrid-pointer-nested
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session.py"
                    "$<TARGET_FILE:qindaqt-wm>"
                    "$<TARGET_FILE:qindaqt-session-probe>"
                    "${QINDAQT_DBUS_RUN_SESSION}"
                    "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                    --plugin-root "${_qindaqt_test_plugin_root}"
                    --expect-hybrid-pointer "${QINDAQT_DOTOOL}"
            )
            set_tests_properties(
                compositor.hybrid-pointer-nested
                PROPERTIES
                    TIMEOUT 45
                    SKIP_RETURN_CODE 77
                    LABELS "integration;compositor;hybrid;input;uinput"
                    RUN_SERIAL TRUE
            )
        endif()

        if(NOT IS_ABSOLUTE "${KDE_INSTALL_BINDIR}"
           AND NOT IS_ABSOLUTE "${KDE_INSTALL_PLUGINDIR}")
            add_test(
                NAME session.installed-plugin-discovery
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test_installed_plugin_discovery.py"
                    "${CMAKE_COMMAND}"
                    "${CMAKE_BINARY_DIR}"
                    "${CMAKE_BINARY_DIR}/tests/session/installed-plugin-smoke"
                    "${KDE_INSTALL_BINDIR}/qindaqt-wm${CMAKE_EXECUTABLE_SUFFIX}"
                    "${KDE_INSTALL_PLUGINDIR}/kwin/plugins/$<TARGET_FILE_NAME:qindaqt_compositor>"
                    "${KDE_INSTALL_PLUGINDIR}/${KDECORATION_PLUGIN_DIR}/$<TARGET_FILE_NAME:qindaqt_decoration>"
                    "$<TARGET_FILE:qindaqt-session-probe>"
                    "${QINDAQT_DBUS_RUN_SESSION}"
                    "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                    --configuration "$<CONFIG>"
            )
            set_tests_properties(
                session.installed-plugin-discovery
                PROPERTIES
                    TIMEOUT 45
                    LABELS "integration;install;compositor;decoration"
                    RUN_SERIAL TRUE
            )
        endif()
    endif()
endif()
