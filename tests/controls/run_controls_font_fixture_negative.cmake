# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_NEGATIVE_EXECUTABLE QINDAQT_MODE
                          QINDAQT_FONT_DIR QINDAQT_RUNTIME_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing font-fixture negative input: ${required}")
    endif()
endforeach()

set(modes missing corrupt wrongfamily)
list(FIND modes "${QINDAQT_MODE}" mode_index)
if(mode_index EQUAL -1)
    message(FATAL_ERROR "QINDAQT_MODE must be missing, corrupt, or wrongfamily")
endif()

set(fixtures_dir "${QINDAQT_RUNTIME_DIR}/font-fixture-negative/${QINDAQT_MODE}")
file(REMOVE_RECURSE "${fixtures_dir}")
file(MAKE_DIRECTORY "${fixtures_dir}")

# Each mode prepares one hostile fixture directory:
#   missing:     the directory holds none of the pinned file names;
#   corrupt:     the first pinned file exists but is empty;
#   wrongfamily: the first pinned file is a valid font declaring another
#                family (the vendored mono face in this repository).
if(QINDAQT_MODE STREQUAL "corrupt")
    file(WRITE "${fixtures_dir}/NotoSans-Regular.ttf" "")
elseif(QINDAQT_MODE STREQUAL "wrongfamily")
    file(COPY_FILE
        "${QINDAQT_FONT_DIR}/NotoSansMono-Regular.ttf"
        "${fixtures_dir}/NotoSans-Regular.ttf"
    )
endif()

set(expected_missing "byte-pinned visual font fixture is missing")
set(expected_corrupt "could not register byte-pinned visual font")
set(expected_wrongfamily "declares families")

execute_process(
    COMMAND "${QINDAQT_NEGATIVE_EXECUTABLE}" "${QINDAQT_MODE}" "${fixtures_dir}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
set(abort_expected_message "${expected_${QINDAQT_MODE}}")
if(result EQUAL 0)
    message(FATAL_ERROR
        "pinDeterministicFonts did not fail closed for mode ${QINDAQT_MODE}\n"
        "${output}${error}"
    )
endif()
if(NOT "${output}${error}" MATCHES "${abort_expected_message}")
    message(FATAL_ERROR
        "Mode ${QINDAQT_MODE} aborted without the expected diagnostic "
        "'${abort_expected_message}':\n${output}${error}"
    )
endif()
message(STATUS "Font fixture failure mode '${QINDAQT_MODE}' failed closed as required")
