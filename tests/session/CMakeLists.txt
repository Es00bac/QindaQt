# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 REQUIRED COMPONENTS Test)

foreach(test_name IN ITEMS installpaths kwincommandbuilder sessioncommandline sessionenvironment)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_compile_features(tst_${test_name} PRIVATE cxx_std_20)
    target_link_libraries(tst_${test_name} PRIVATE QindaQt::SessionSupport Qt6::Test)
    set_target_properties(tst_${test_name} PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(tst_${test_name})
    add_test(NAME session.${test_name} COMMAND tst_${test_name})
endforeach()

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
set_tests_properties(
    session.nested-scenario-parser
    PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS unit
)

add_test(
    NAME session.python-syntax
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PYTHONPYCACHEPREFIX=${CMAKE_CURRENT_BINARY_DIR}/python-cache"
        "${Python3_EXECUTABLE}" -m py_compile
        "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_nested_session_unit.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_parent_wayland_session.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_installed_plugin_discovery.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_plugin_unload.py"
)
set_tests_properties(session.python-syntax PROPERTIES LABELS unit)

find_program(QINDAQT_DBUS_RUN_SESSION dbus-run-session)
find_program(QINDAQT_KWIN_WAYLAND kwin_wayland)
find_program(QINDAQT_XDPYINFO xdpyinfo)
find_program(QINDAQT_XWAYLAND Xwayland)
find_program(QINDAQT_WESTON weston)

# AGENT-CONTRACT: qindaqt-wm defaults to its installed KDE plugin root. Every
# build-tree integration test must name the transient root explicitly.
set(_qindaqt_test_plugin_root "${CMAKE_BINARY_DIR}/plugins")

if(QINDAQT_DBUS_RUN_SESSION AND QINDAQT_KWIN_WAYLAND AND QINDAQT_XDPYINFO AND QINDAQT_XWAYLAND)
    qt_add_executable(
        qindaqt-session-probe
        compositordevelopmentworkflow.cpp
        compositordevelopmentworkflow.h
        compositorprobeclient.cpp
        compositorprobeclient.h
        compositorworkflow.cpp
        compositorworkflow.h
        sessionprobe.cpp
    )
    target_link_libraries(qindaqt-session-probe PRIVATE Qt6::Core Qt6::DBus Qt6::Gui)
    set_target_properties(qindaqt-session-probe PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt-session-probe)

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
        qt_add_executable(
            qindaqt-plugin-unload-probe
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
                    "$<TARGET_FILE:qindaqt-session-probe>"
                    "${QINDAQT_DBUS_RUN_SESSION}"
                    "${PROJECT_SOURCE_DIR}/tests/scenarios/single-1080p.json"
                    --configuration "$<CONFIG>"
            )
            set_tests_properties(
                session.installed-plugin-discovery
                PROPERTIES TIMEOUT 45 LABELS "integration;install;compositor" RUN_SERIAL TRUE
            )
        endif()
    endif()
endif()
