# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()
if(NOT DEFINED SCAN_ROOT)
    set(SCAN_ROOT "${SOURCE_ROOT}")
endif()

set(consumer_roots
    "${SCAN_ROOT}/src/session_supervisor"
    "${SCAN_ROOT}/src/shell/power_applet"
    "${SCAN_ROOT}/src/apps/settings/power"
    "${SCAN_ROOT}/src/shell/runtime/powerappletcomposition.cpp"
    "${SCAN_ROOT}/src/shell/runtime/powerappletcomposition.h")
foreach(root IN LISTS consumer_roots)
    if(IS_DIRECTORY "${root}")
        file(GLOB_RECURSE files
            "${root}/*.cpp" "${root}/*.h" "${root}/*.qml")
    elseif(EXISTS "${root}")
        set(files "${root}")
    else()
        continue()
    endif()
    foreach(file IN LISTS files)
        file(READ "${file}" contents)
        if(contents MATCHES "org\\.freedesktop\\.login1|/org/freedesktop/login1|CanPowerOff|CanReboot|CanSuspend|org\\.freedesktop\\.ScreenSaver|/ScreenSaver")
            message(FATAL_ERROR
                "Direct session authority escaped session_actions: ${file}")
        endif()
    endforeach()
endforeach()

if("${SCAN_ROOT}" STREQUAL "${SOURCE_ROOT}")
    file(READ
        "${SOURCE_ROOT}/src/services/session_actions/src/session_actions_client.cpp"
        action_client)
    foreach(required IN ITEMS
            "org.freedesktop.login1"
            "org.freedesktop.ScreenSaver"
            "org.qindaqt.Session1")
        if(NOT action_client MATCHES "${required}")
            message(FATAL_ERROR "session_actions is missing ${required}")
        endif()
    endforeach()
endif()
