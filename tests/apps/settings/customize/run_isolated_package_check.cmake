# SPDX-License-Identifier: GPL-3.0-or-later

# Runs the shared Settings package harness for
# qindaqt.settings-customize-installed-route so that its withheld-module
# poisons stay truthful on a host that also has QindaQt installed.
#
# AGENT-NOTE: check_installed_routes.cmake withholds staged route modules and
# requires the relocated qindaqt-settings to fail root construction (exit 3).
# Qt always keeps its own QML directory (QLibraryInfo::QmlImportsPath) in the
# engine import list, so when that directory also holds an installed QindaQt a
# withheld staged module resolves to the host copy, the root constructs, and
# the poison run ends in "Process terminated due to timeout" instead. The
# harness then fails before any Customize step. docs/wiki/apps/customize-settings.md
# records this row's isolation; docs/wiki/development/testing-harness.md records
# the same limitation for the sibling installed rows.
#
# AGENT-GUARD: Hide only the host QindaQt directory inside Qt's QML import
# directory, only inside a private mount namespace of this test process, and
# run the shared harness unchanged. Masking Qt's own modules breaks every route;
# making the build tree or /tmp read-only breaks the staged install and the
# harness's short poison runtime directory; editing the shared harness from
# this row would fork the Settings package contract.

foreach(required IN ITEMS BUILD_DIRECTORY INSTALL_PREFIX INSTALL_BINDIR
                          INSTALL_DATADIR INSTALL_QMLDIR
                          SETTINGS_EXECUTABLE_NAME ROUTE_CHECK
                          PACKAGE_CHECK HOST_QML_IMPORT_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing isolated Customize package input: ${required}")
    endif()
endforeach()

cmake_path(IS_ABSOLUTE HOST_QML_IMPORT_DIR host_qml_is_absolute)
if(NOT host_qml_is_absolute OR NOT IS_DIRECTORY "${HOST_QML_IMPORT_DIR}/QtQuick")
    # A wrong path would silently skip isolation and reintroduce the timeout.
    message(FATAL_ERROR
        "host Qt QML import directory is not Qt's module root: ${HOST_QML_IMPORT_DIR}")
endif()
set(host_qindaqt_qml "${HOST_QML_IMPORT_DIR}/QindaQt")

set(forwarded_inputs)
foreach(input IN ITEMS BUILD_DIRECTORY INSTALL_PREFIX INSTALL_BINDIR
                       INSTALL_DATADIR INSTALL_QMLDIR SETTINGS_EXECUTABLE_NAME
                       CONFIGURATION ROUTE_CHECK)
    if(DEFINED ${input})
        list(APPEND forwarded_inputs "-D${input}=${${input}}")
    endif()
endforeach()

function(run_package_check)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${forwarded_inputs} -P "${PACKAGE_CHECK}"
        RESULT_VARIABLE package_status
    )
    if(NOT package_status EQUAL 0)
        message(FATAL_ERROR
            "shared Settings package harness failed (${package_status})")
    endif()
endfunction()

if(QINDAQT_HOST_QML_MASKED)
    file(GLOB visible_host_modules "${host_qindaqt_qml}/*")
    if(visible_host_modules)
        message(FATAL_ERROR
            "host QindaQt QML modules remain visible inside the isolation "
            "namespace: ${visible_host_modules}")
    endif()
    run_package_check()
elseif(NOT IS_DIRECTORY "${host_qindaqt_qml}")
    run_package_check()
else()
    if(NOT BWRAP_EXECUTABLE OR NOT EXISTS "${BWRAP_EXECUTABLE}")
        message(FATAL_ERROR
            "${host_qindaqt_qml} holds installed QindaQt QML modules that "
            "satisfy the withheld-module poisons; bubblewrap is required to hide "
            "them for this row but is unavailable (${BWRAP_EXECUTABLE})")
    endif()
    execute_process(
        COMMAND "${BWRAP_EXECUTABLE}"
                --bind / /
                --dev /dev
                --proc /proc
                --die-with-parent
                --tmpfs "${host_qindaqt_qml}"
                "${CMAKE_COMMAND}" ${forwarded_inputs}
                "-DPACKAGE_CHECK=${PACKAGE_CHECK}"
                "-DHOST_QML_IMPORT_DIR=${HOST_QML_IMPORT_DIR}"
                -DQINDAQT_HOST_QML_MASKED=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE isolated_status
    )
    if(NOT isolated_status EQUAL 0)
        message(FATAL_ERROR
            "isolated Settings package check failed (${isolated_status}) with "
            "host QindaQt QML hidden from ${host_qindaqt_qml}")
    endif()
endif()
