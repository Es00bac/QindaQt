# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_FILE OR NOT EXISTS "${SOURCE_FILE}")
    message(FATAL_ERROR "File Manager menu-export test source is unavailable")
endif()

file(READ "${SOURCE_FILE}" source)

# AGENT-NOTE: P2-01 regression proof. A behavior test alone cannot expose that
# the rejected candidate omitted hostile real-process rows, so this registered
# policy requires both identity mismatches and their live child-PID boundary.
foreach(required IN ITEMS
        "QTest::newRow(\"mismatched-pid\")"
        "QTest::newRow(\"mismatched-window-id\")"
        "fileManager.processId()"
        "identityVariant == 1"
        "identityVariant == 2")
    string(FIND "${source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "File Manager real-process menu test is missing required identity variant: ${required}")
    endif()
endforeach()
