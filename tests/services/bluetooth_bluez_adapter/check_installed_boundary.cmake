# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BINARY_ROOT STAGE_ROOT ADAPTER_LIBRARY_NAME
                          INSTALL_LIBDIR INSTALL_INCLUDEDIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Bluetooth B1 installed-boundary input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BINARY_ROOT OUTPUT_VARIABLE binary_root)
cmake_path(NORMAL_PATH STAGE_ROOT OUTPUT_VARIABLE stage_root)
cmake_path(IS_PREFIX binary_root "${stage_root}" NORMALIZE stage_is_in_build)
if(NOT stage_is_in_build OR stage_root STREQUAL binary_root)
    message(FATAL_ERROR "Bluetooth B1 stage must be a child of the build tree")
endif()

file(REMOVE_RECURSE "${stage_root}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${binary_root}"
            --prefix "${stage_root}" --component QindaQtBluetoothB1
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
        "Bluetooth B1 component install failed:\n${install_output}${install_error}")
endif()

set(adapter_library "${stage_root}/${INSTALL_LIBDIR}/${ADAPTER_LIBRARY_NAME}")
set(header_root
    "${stage_root}/${INSTALL_INCLUDEDIR}/qindaqt/services/bluetooth_bluez_adapter")
set(expected_headers
    "${header_root}/bluez_adapter_backend.h"
    "${header_root}/bluez_backend_mode.h"
)
if(NOT EXISTS "${adapter_library}")
    message(FATAL_ERROR "Installed BlueZ adapter archive is missing")
endif()
foreach(header IN LISTS expected_headers)
    if(NOT EXISTS "${header}")
        message(FATAL_ERROR "Installed BlueZ adapter header is missing: ${header}")
    endif()
endforeach()

file(GLOB_RECURSE installed_headers LIST_DIRECTORIES false "${header_root}/*.h")
list(SORT installed_headers)
list(SORT expected_headers)
if(NOT installed_headers STREQUAL expected_headers)
    message(FATAL_ERROR
        "Installed BlueZ adapter leaked or omitted headers: ${installed_headers}")
endif()

foreach(header IN LISTS installed_headers)
    file(READ "${header}" content)
    if(content MATCHES "bluez_transport|bluez_object_store|BluezManagedObjects")
        message(FATAL_ERROR "Installed public header leaks private BlueZ state: ${header}")
    endif()
endforeach()

message(STATUS "Bluetooth B1 component exposes only its archive and public port adapter headers")
