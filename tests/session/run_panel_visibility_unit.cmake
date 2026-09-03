# SPDX-License-Identifier: GPL-3.0-or-later

foreach(_required IN ITEMS PYTHON VALIDATOR SETTLEMENT_TEST TMP_ROOT)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "missing panel visibility unit input: ${_required}")
    endif()
endforeach()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PYTHONDONTWRITEBYTECODE=1"
        "TMPDIR=${TMP_ROOT}"
        "${PYTHON}" "${VALIDATOR}"
    RESULT_VARIABLE _validator_status
)
if(NOT _validator_status EQUAL 0)
    message(FATAL_ERROR "panel visibility validator controls failed: ${_validator_status}")
endif()

execute_process(
    COMMAND "${SETTLEMENT_TEST}"
    RESULT_VARIABLE _settlement_status
)
if(NOT _settlement_status EQUAL 0)
    message(FATAL_ERROR "panel visibility settlement controls failed: ${_settlement_status}")
endif()
