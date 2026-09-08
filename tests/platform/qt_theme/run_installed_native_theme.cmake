# SPDX-License-Identifier: GPL-3.0-or-later
set(stage "${BUILD_ROOT}/tests/platform/qt-theme-stage")
file(REMOVE_RECURSE "${stage}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BUILD_ROOT}" --prefix "${stage}"
    --component QtPlatformTheme RESULT_VARIABLE installed)
if(NOT installed EQUAL 0)
    message(FATAL_ERROR "Qt platform theme stage failed: ${installed}")
endif()
execute_process(COMMAND python3 "${SOURCE_ROOT}/tests/platform/qt_theme/run_native_theme.py"
    "${PROBE}" "${stage}/${INSTALL_LIBDIR}/qt6/plugins" "${SOURCE_ROOT}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Qt platform theme probe failed: ${result}")
endif()
