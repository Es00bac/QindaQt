# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED QINDAQT_BUILD_DIR OR NOT DEFINED QINDAQT_INSTALL_PREFIX)
    message(FATAL_ERROR "build directory and install prefix are required")
endif()

file(REMOVE_RECURSE "${QINDAQT_INSTALL_PREFIX}")
foreach(component IN ITEMS QindaQtNetworkSecretAgent)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" --install "${QINDAQT_BUILD_DIR}"
            --prefix "${QINDAQT_INSTALL_PREFIX}" --component "${component}"
        RESULT_VARIABLE install_result
        OUTPUT_VARIABLE install_output
        ERROR_VARIABLE install_error
    )
    if(NOT install_result EQUAL 0)
        message(FATAL_ERROR
            "install ${component} failed: ${install_output}${install_error}")
    endif()
endforeach()

set(agent
    "${QINDAQT_INSTALL_PREFIX}/bin/qindaqt-network-secret-agent${QINDAQT_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${agent}" OR IS_SYMLINK "${agent}")
    message(FATAL_ERROR "installed secret-agent executable is absent or a symlink")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        QT_QPA_PLATFORM=offscreen
        QT_QUICK_BACKEND=software
        DISPLAY=
        WAYLAND_DISPLAY=
        DBUS_SESSION_BUS_ADDRESS=
        DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent
        "${agent}" --check-package
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
        "relocated secret-agent check failed: ${run_output}${run_error}")
endif()
