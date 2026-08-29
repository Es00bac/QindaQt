# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_INSTALL_PREFIX
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_POWER_CLIENT_SOURCE_DIRECTORY
                          QINDAQT_POWER_SERVICE_SOURCE_DIRECTORY
                          QINDAQT_POWER_PROTOCOL_SOURCE_DIRECTORY
                          QINDAQT_INSTALL_INCLUDEDIR
                          QINDAQT_POWER_CLIENT_LIBRARY QINDAQT_POWER_SERVICE_LIBRARY
                          QINDAQT_POWER_PROTOCOL_LIBRARY
                          QINDAQT_POWER_SERVICE_EXECUTABLE
                          QINDAQT_EXPECTED_INSTALL_EXECUTABLE
                          QINDAQT_DBUS_SERVICE_FILE QINDAQT_SYSTEMD_UNIT_FILE
                          QINDAQT_QT6_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed Power package input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a Power package stage outside the test build tree")
endif()

file(REMOVE_RECURSE "${install_prefix}")

file(
    COPY "${QINDAQT_POWER_CLIENT_SOURCE_DIRECTORY}/include/"
    DESTINATION "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
)
file(
    COPY "${QINDAQT_POWER_SERVICE_SOURCE_DIRECTORY}/include/"
    DESTINATION "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
)
file(
    COPY "${QINDAQT_POWER_PROTOCOL_SOURCE_DIRECTORY}/include/"
    DESTINATION "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
)

foreach(relative IN ITEMS
        "qindaqt/services/power_protocol/power_types.h"
        "qindaqt/services/power_protocol/power_limits.h"
        "qindaqt/services/power_protocol/power_validation.h"
        "qindaqt/services/power_client/power_client.h"
        "qindaqt/services/power_client/power_transport.h"
        "qindaqt/services/power_client/qt_power_transport.h"
        "qindaqt/services/power_service/power_collaborators.h"
        "qindaqt/services/power_service/power_service_coordinator.h"
        "qindaqt/services/power_service/resident_power_service.h"
        "qindaqt/services/power_service/unavailable_power_collaborators.h")
    if(NOT EXISTS "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}/${relative}")
        message(FATAL_ERROR "Installed Power public header is missing ${relative}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_POWER_SERVICE_EXECUTABLE OUTPUT_VARIABLE service_executable)
cmake_path(NORMAL_PATH QINDAQT_EXPECTED_INSTALL_EXECUTABLE OUTPUT_VARIABLE installed_executable)
cmake_path(NORMAL_PATH QINDAQT_DBUS_SERVICE_FILE OUTPUT_VARIABLE dbus_service_file)
cmake_path(NORMAL_PATH QINDAQT_SYSTEMD_UNIT_FILE OUTPUT_VARIABLE systemd_unit_file)

file(READ "${dbus_service_file}" dbus_service)
string(REGEX MATCH "Name=org.qindaqt.Power1" dbus_name "${dbus_service}")
if(NOT dbus_name)
    message(FATAL_ERROR "Activation descriptor lost its reserved name: ${dbus_service}")
endif()
string(REGEX MATCH "SystemdService=qindaqt-power-service.service" dbus_systemd "${dbus_service}")
if(NOT dbus_systemd)
    message(FATAL_ERROR "Activation descriptor lost its systemd pairing: ${dbus_service}")
endif()
string(REGEX MATCH "Exec=[^ \n]*" dbus_exec "${dbus_service}")
string(REGEX REPLACE "^Exec=" "" dbus_executable "${dbus_exec}")
cmake_path(NORMAL_PATH dbus_executable OUTPUT_VARIABLE dbus_executable_path)
if(NOT dbus_executable_path STREQUAL installed_executable)
    message(FATAL_ERROR
        "Activation descriptor Exec '${dbus_executable_path}' does not resolve to the"
        " installed executable '${installed_executable}'")
endif()

file(READ "${systemd_unit_file}" systemd_unit)
string(REGEX MATCH "Type=dbus" systemd_type "${systemd_unit}")
if(NOT systemd_type)
    message(FATAL_ERROR "Systemd user unit is not D-Bus activated: ${systemd_unit}")
endif()
string(REGEX MATCH "BusName=org.qindaqt.Power1" systemd_bus_name "${systemd_unit}")
if(NOT systemd_bus_name)
    message(FATAL_ERROR "Systemd user unit lost its bus name: ${systemd_unit}")
endif()
string(REGEX MATCH "ExecStart=[^ \n]*" systemd_exec "${systemd_unit}")
string(REGEX REPLACE "^ExecStart=" "" systemd_executable "${systemd_exec}")
cmake_path(NORMAL_PATH systemd_executable OUTPUT_VARIABLE systemd_executable_path)
if(NOT systemd_executable_path STREQUAL installed_executable)
    message(FATAL_ERROR
        "Systemd unit ExecStart '${systemd_executable_path}' does not resolve to the"
        " installed executable '${installed_executable}'")
endif()
if(NOT EXISTS "${service_executable}")
    message(FATAL_ERROR "Packaged Power executable does not exist: ${service_executable}")
endif()

set(consumer_build "${install_prefix}/consumer-build")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}"
        -S "${QINDAQT_CONSUMER_SOURCE_DIRECTORY}"
        -B "${consumer_build}"
        "-DCMAKE_BUILD_TYPE=${QINDAQT_BUILD_TYPE}"
        "-DQt6_DIR=${QINDAQT_QT6_DIR}"
        "-DQINDAQT_STAGE_INCLUDE_DIR=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_POWER_CLIENT_LIBRARY=${QINDAQT_POWER_CLIENT_LIBRARY}"
        "-DQINDAQT_POWER_SERVICE_LIBRARY=${QINDAQT_POWER_SERVICE_LIBRARY}"
        "-DQINDAQT_POWER_PROTOCOL_LIBRARY=${QINDAQT_POWER_PROTOCOL_LIBRARY}"
        "-DQINDAQT_CONSUMER_SOURCE=${QINDAQT_CONSUMER_SOURCE}"
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR "Installed Power consumer configure failed:\n${configure_output}${configure_error}")
endif()

execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 1
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR "Installed Power consumer build failed:\n${build_output}${build_error}")
endif()

execute_process(
    COMMAND "${QINDAQT_CMAKE}" -E env "QT_QPA_PLATFORM=offscreen"
            "${consumer_build}/qindaqt_installed_power_consumer"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Power C++ consumer exited ${consumer_status}:\n"
        "${consumer_output}${consumer_error}")
endif()

message(STATUS "Installed Power package and consumer passed all checks successfully")
