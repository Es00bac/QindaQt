# SPDX-License-Identifier: GPL-3.0-or-later

# This installed-boundary row depends on the plugin and probe targets created by
# the enclosing session test configuration. Keep it included at that boundary.
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
