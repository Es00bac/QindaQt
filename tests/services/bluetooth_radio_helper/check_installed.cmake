# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS BINARY_ROOT STAGE_ROOT CONSUMER_SOURCE RADIO_LIBRARY INSTALL_LIBDIR INSTALL_INCLUDEDIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing radio stage input: ${required}")
    endif()
endforeach()
cmake_path(IS_PREFIX BINARY_ROOT "${STAGE_ROOT}" NORMALIZE safe_stage)
if(NOT safe_stage OR STAGE_ROOT STREQUAL BINARY_ROOT)
    message(FATAL_ERROR "Radio stage must be inside owning build")
endif()
file(REMOVE_RECURSE "${STAGE_ROOT}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BINARY_ROOT}"
    --prefix "${STAGE_ROOT}" --component QindaQtBluetoothB1 RESULT_VARIABLE installed)
if(NOT installed EQUAL 0)
    message(FATAL_ERROR "Radio component staging failed")
endif()
set(header "${STAGE_ROOT}/${INSTALL_INCLUDEDIR}/qindaqt/services/bluetooth_radio_helper/qt_radio_power_port.h")
file(SHA256 "${header}" original)
set(consumer "${STAGE_ROOT}/consumer")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${CONSUMER_SOURCE}" -B "${consumer}" -G Ninja
    "-DSTAGE_INCLUDE=${STAGE_ROOT}/${INSTALL_INCLUDEDIR}"
    "-DRADIO_ARCHIVE=${STAGE_ROOT}/${INSTALL_LIBDIR}/${RADIO_LIBRARY}" RESULT_VARIABLE configured)
if(NOT configured EQUAL 0)
    message(FATAL_ERROR "Radio installed consumer configure failed")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${consumer}" -- -j24 -l24 RESULT_VARIABLE positive)
if(NOT positive EQUAL 0)
    message(FATAL_ERROR "Radio installed header/archive consumer failed")
endif()
file(RENAME "${header}" "${header}.withheld")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${consumer}" --clean-first -- -j24 -l24
    RESULT_VARIABLE poisoned OUTPUT_VARIABLE poison_out ERROR_VARIABLE poison_err)
file(RENAME "${header}.withheld" "${header}")
file(SHA256 "${header}" restored_hash)
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${consumer}" -- -j24 -l24 RESULT_VARIABLE restored)
file(WRITE "${STAGE_ROOT}/header-withheld.log" "${poison_out}${poison_err}")
if(poisoned EQUAL 0 OR NOT "${poison_out}${poison_err}" MATCHES "qt_radio_power_port.h"
    OR NOT original STREQUAL restored_hash OR NOT restored EQUAL 0)
    message(FATAL_ERROR "Radio staged-header poison/restoration did not prove closure")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent-radio-test"
    "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent-radio-test"
    "${consumer}/installed_radio_port" RESULT_VARIABLE ran)
if(NOT ran EQUAL 0)
    message(FATAL_ERROR "Radio installed consumer execution failed")
endif()
message(STATUS "Actual staged radio header/archive consumer, withhold and restore passed")
