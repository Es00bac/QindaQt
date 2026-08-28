if(NOT INSTALL_PREFIX MATCHES "/installed-editor-stage$")
    message(FATAL_ERROR "refusing unsafe staged-install prefix: '${INSTALL_PREFIX}'")
endif()
file(REMOVE_RECURSE "${INSTALL_PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIRECTORY}"
            --prefix "${INSTALL_PREFIX}" --config "${CONFIGURATION}"
            --component TextEditor
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "staged install failed: ${install_output}${install_error}")
endif()

set(editor "${INSTALL_PREFIX}/${INSTALL_BINDIR}/qindaqt-editor")
set(desktop "${INSTALL_PREFIX}/${INSTALL_DATADIR}/applications/org.qindaqt.TextEditor.desktop")
set(theme_dir "${INSTALL_PREFIX}/${INSTALL_DATADIR}/qindaqt/themes")
set(theme_ids
    qinda-dark
    qinda-light
    qinda-dusk
    qinda-macos
    qinda-high-contrast
)
set(required_payload "${editor}" "${desktop}")
foreach(theme_id IN LISTS theme_ids)
    list(APPEND required_payload "${theme_dir}/${theme_id}.json")
endforeach()
foreach(required IN LISTS required_payload)
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "installed editor payload missing: ${required}")
    endif()
endforeach()

file(READ "${desktop}" desktop_contents)
foreach(required_entry
        "Type=Application"
        "Name=QindaQt Text Editor"
        "Exec=qindaqt-editor %f"
        "MimeType=text/plain;"
        "Terminal=false")
    string(FIND "${desktop_contents}" "${required_entry}" entry_position)
    if(entry_position EQUAL -1)
        message(FATAL_ERROR "installed desktop metadata is missing: ${required_entry}")
    endif()
endforeach()

foreach(theme_id IN LISTS theme_ids)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "QT_QPA_PLATFORM=offscreen"
                "XDG_DATA_DIRS=${INSTALL_PREFIX}/${INSTALL_DATADIR}"
                "${editor}" --theme "${theme_id}" --check-theme
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "installed editor ${theme_id} probe failed: ${probe_output}${probe_error}"
        )
    endif()
    string(STRIP "${probe_output}" probe_output)
    if(NOT probe_output STREQUAL "${theme_id} qst-1")
        message(FATAL_ERROR
            "installed editor ${theme_id} probe returned '${probe_output}'"
        )
    endif()
endforeach()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}"
            "${RUNTIME_PROBE}"
            --executable "${editor}"
            --data-dir "${INSTALL_PREFIX}/${INSTALL_DATADIR}"
            --startup-limit-ms 400
            --pss-limit-kib 65536
    RESULT_VARIABLE runtime_result
    OUTPUT_VARIABLE runtime_output
    ERROR_VARIABLE runtime_error
)
if(NOT runtime_result EQUAL 0)
    message(FATAL_ERROR "installed editor runtime probe failed: ${runtime_output}${runtime_error}")
endif()
message(STATUS "installed editor runtime evidence: ${runtime_output}")
