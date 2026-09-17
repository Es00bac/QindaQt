# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-GUARD: every QML row in this directory runs `qmltestrunner -import
# <build>/qml`. When the `*_qmlplugin` target has not been built, that import
# path holds no usable module and Qt silently falls back to the *installed*
# QindaQt.Shell.GlobalMenu under the system QML prefix — so the row tests
# whatever bytes the last package installed and reports a pass. That has
# already produced false confidence once: a raised entry-count default read
# back as the old value because the installed module answered instead of the
# build tree (ADR-0188, memory `qindaqt-installed-qml-module-fallback`).
#
# This fixture fails the rows closed instead. It is a ctest FIXTURES_SETUP, so
# a missing build-tree module is one explicit failure naming the target to
# build, rather than several suites quietly passing against stale bytes.

if(NOT DEFINED MODULE_DIR)
    message(FATAL_ERROR "MODULE_DIR is required")
endif()
if(NOT DEFINED PLUGIN_FILE)
    message(FATAL_ERROR "PLUGIN_FILE is required")
endif()

if(NOT EXISTS "${MODULE_DIR}/qmldir")
    message(FATAL_ERROR
        "the build tree has no QindaQt.Shell.GlobalMenu module at "
        "${MODULE_DIR}: build the qindaqt_global_menu_qmlplugin target before "
        "running these rows, or they resolve the installed module instead")
endif()
if(NOT EXISTS "${PLUGIN_FILE}")
    message(FATAL_ERROR
        "the build tree has no GlobalMenu QML plugin at ${PLUGIN_FILE}: build "
        "the qindaqt_global_menu_qmlplugin target before running these rows")
endif()

file(GLOB module_qml "${MODULE_DIR}/qml/*.qml")
if(module_qml STREQUAL "")
    message(FATAL_ERROR
        "the build-tree GlobalMenu module at ${MODULE_DIR} carries no QML "
        "files; the module directory map did not populate")
endif()

message(STATUS "GlobalMenu QML module resolves from the build tree: ${MODULE_DIR}")
