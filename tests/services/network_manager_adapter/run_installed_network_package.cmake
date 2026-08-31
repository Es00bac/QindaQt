# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS
        QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_INSTALL_PREFIX
        QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_INSTALL_LIBDIR
        QINDAQT_INSTALL_INCLUDEDIR QINDAQT_NETWORK_PROTOCOL_LIBRARY_NAME
        QINDAQT_NETWORK_MODEL_LIBRARY_NAME QINDAQT_NETWORK_CLIENT_LIBRARY_NAME
        QINDAQT_NETWORK_QT_TRANSPORT_LIBRARY_NAME
        QINDAQT_NETWORK_SERVICE_LIBRARY_NAME
        QINDAQT_NETWORK_MANAGER_ADAPTER_LIBRARY_NAME
        QINDAQT_NETWORK_ACTIVATION_TEST QINDAQT_QT6_DIR QINDAQT_GENERATOR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing installed Network N1 input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace an N1 stage outside the test build tree")
endif()
file(REMOVE_RECURSE "${install_prefix}")

foreach(component IN ITEMS QindaQtNetworkN0 QindaQtNetworkN1)
    set(command "${QINDAQT_CMAKE}" --install "${build_directory}"
                --prefix "${install_prefix}" --component "${component}")
    if(DEFINED QINDAQT_CONFIGURATION AND NOT QINDAQT_CONFIGURATION STREQUAL "")
        list(APPEND command --config "${QINDAQT_CONFIGURATION}")
    endif()
    execute_process(
        COMMAND ${command}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Network ${component} install failed:\n${output}${error}")
    endif()
endforeach()

set(include_dir "${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}")
set(lib_dir "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}")
set(service_executable "${install_prefix}/bin/qindaqt-network-service")
foreach(path IN ITEMS
        "${include_dir}/qindaqt/services/network_qt_transport/qt_network_transport.h"
        "${include_dir}/qindaqt/services/network_service/resident_network_service.h"
        "${include_dir}/qindaqt/services/network_manager_adapter/network_manager_backend.h"
        "${lib_dir}/${QINDAQT_NETWORK_QT_TRANSPORT_LIBRARY_NAME}"
        "${lib_dir}/${QINDAQT_NETWORK_SERVICE_LIBRARY_NAME}"
        "${lib_dir}/${QINDAQT_NETWORK_MANAGER_ADAPTER_LIBRARY_NAME}"
        "${service_executable}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Installed Network N1 artifact is missing: ${path}")
    endif()
endforeach()
file(GLOB_RECURSE activation_descriptors
     "${install_prefix}/*/org.qindaqt.Network1.service"
     "${install_prefix}/*/qindaqt-network-service.service"
     "${install_prefix}/*/org.qindaqt.Network1.xml")
list(LENGTH activation_descriptors descriptor_count)
if(NOT descriptor_count EQUAL 3)
    message(FATAL_ERROR
        "Expected three installed Network1 activation/interface artifacts, got ${descriptor_count}")
endif()

set(consumer_build "${install_prefix}/consumer-build")
execute_process(
    COMMAND
        "${QINDAQT_CMAKE}"
        -S "${QINDAQT_CONSUMER_SOURCE_DIRECTORY}"
        -B "${consumer_build}"
        -G "${QINDAQT_GENERATOR}"
        "-DCMAKE_BUILD_TYPE=${QINDAQT_BUILD_TYPE}"
        "-DQt6_DIR=${QINDAQT_QT6_DIR}"
        "-DQINDAQT_STAGE_INCLUDE_DIR=${include_dir}"
        "-DQINDAQT_NETWORK_PROTOCOL_LIBRARY=${lib_dir}/${QINDAQT_NETWORK_PROTOCOL_LIBRARY_NAME}"
        "-DQINDAQT_NETWORK_MODEL_LIBRARY=${lib_dir}/${QINDAQT_NETWORK_MODEL_LIBRARY_NAME}"
        "-DQINDAQT_NETWORK_CLIENT_LIBRARY=${lib_dir}/${QINDAQT_NETWORK_CLIENT_LIBRARY_NAME}"
        "-DQINDAQT_NETWORK_QT_TRANSPORT_LIBRARY=${lib_dir}/${QINDAQT_NETWORK_QT_TRANSPORT_LIBRARY_NAME}"
        "-DQINDAQT_NETWORK_SERVICE_LIBRARY=${lib_dir}/${QINDAQT_NETWORK_SERVICE_LIBRARY_NAME}"
        "-DQINDAQT_NETWORK_MANAGER_ADAPTER_LIBRARY=${lib_dir}/${QINDAQT_NETWORK_MANAGER_ADAPTER_LIBRARY_NAME}"
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Network N1 consumer configure failed:\n${configure_output}${configure_error}")
endif()
execute_process(
    COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 2
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Network N1 consumer build failed:\n${build_output}${build_error}")
endif()
execute_process(
    COMMAND "${consumer_build}/qindaqt_installed_network_n1_consumer"
    RESULT_VARIABLE consumer_status
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Network N1 consumer failed:\n${consumer_output}${consumer_error}")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "QINDAQT_NETWORK_SERVICE_UNDER_TEST=${service_executable}"
        "${QINDAQT_NETWORK_ACTIVATION_TEST}"
        activatesAgainstPrivateUnavailableNetworkManager
    RESULT_VARIABLE lifecycle_status
    OUTPUT_VARIABLE lifecycle_output
    ERROR_VARIABLE lifecycle_error
)
if(NOT lifecycle_status EQUAL 0)
    message(FATAL_ERROR
        "Installed Network1 activation lifecycle failed:\n${lifecycle_output}${lifecycle_error}")
endif()

message(STATUS "Installed Network N1 consumer and service lifecycle passed")
