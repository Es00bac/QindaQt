# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: the Clipboard applet QML rows execute through this driver so
# their build prerequisites are explicit and verified before qmltestrunner
# starts. Registering a qmltestrunner add_test() alone hides the QML plugin
# targets behind an implicit full build; a focused build of the declared test
# targets then fails all three rows at import time. The driver builds
# QINDAQT_PREREQUISITE_TARGET first, so every plugin and manifest binary the
# rows need exists even in a partial build.

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY
                          QINDAQT_CONFIGURATION QINDAQT_PREREQUISITE_TARGET
                          QINDAQT_QMLTESTRUNNER QINDAQT_QML_IMPORT_PATH
                          QINDAQT_TEST_INPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Clipboard applet QML test input: ${required}")
    endif()
endforeach()

set(build_command "${QINDAQT_CMAKE}" --build "${QINDAQT_BUILD_DIRECTORY}"
                  --target "${QINDAQT_PREREQUISITE_TARGET}")
if(NOT QINDAQT_CONFIGURATION STREQUAL "")
    list(APPEND build_command --config "${QINDAQT_CONFIGURATION}")
endif()
execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
            "Clipboard applet QML test prerequisites failed to build:\n"
            "${build_output}${build_error}")
endif()

execute_process(
    COMMAND
        "${QINDAQT_CMAKE}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QML2_IMPORT_PATH=${QINDAQT_QML_IMPORT_PATH}"
        "${QINDAQT_QMLTESTRUNNER}"
        -import "${QINDAQT_QML_IMPORT_PATH}"
        -input "${QINDAQT_TEST_INPUT}"
    RESULT_VARIABLE test_status
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)
if(NOT test_status EQUAL 0)
    message(FATAL_ERROR
            "Clipboard applet QML test ${QINDAQT_TEST_INPUT} exited ${test_status}:\n"
            "${test_output}${test_error}")
endif()
