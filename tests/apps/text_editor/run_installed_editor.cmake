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
# ADR-0116: the TextEditor component ships no per-app theme catalog; the Qt
# platform theme owns appearance. The payload is the executable plus desktop
# metadata only.
foreach(required IN ITEMS "${editor}" "${desktop}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "installed editor payload missing: ${required}")
    endif()
endforeach()
if(EXISTS "${INSTALL_PREFIX}/${INSTALL_DATADIR}/qindaqt/themes")
    message(FATAL_ERROR
        "installed editor payload must not ship a per-app theme catalog (ADR-0116)")
endif()

file(READ "${desktop}" desktop_contents)
foreach(required_entry
        "Type=Application"
        "Name=QindaQt Text Editor"
        "Exec=qindaqt-editor %F"
        "Icon=org.qindaqt.TextEditor"
        "MimeType=text/plain;"
        "Terminal=false")
    string(FIND "${desktop_contents}" "${required_entry}" entry_position)
    if(entry_position EQUAL -1)
        message(FATAL_ERROR "installed desktop metadata is missing: ${required_entry}")
    endif()
endforeach()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}"
            "${RUNTIME_PROBE}"
            --executable "${editor}"
            --data-dir "${INSTALL_PREFIX}/${INSTALL_DATADIR}"
            --scratch-dir "${SCRATCH_ROOT}"
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
