# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_FILE OR NOT EXISTS "${SOURCE_FILE}")
    message(FATAL_ERROR "Terminal menu-export test source is unavailable")
endif()

file(READ "${SOURCE_FILE}" source)

# AGENT-NOTE: regression proof mirroring the File Manager policy row. A
# behavior test alone cannot expose that a candidate omitted hostile
# real-process rows, so this registered policy requires both identity
# mismatches, their live child-PID boundary, and the fail-closed rows.
foreach(required IN ITEMS
        "QTest::newRow(\"mismatched-pid\")"
        "QTest::newRow(\"mismatched-window-id\")"
        "terminal.processId()"
        "identityVariant == 1"
        "identityVariant == 2"
        "staysFailClosedUntilRegistrarArrives"
        "failsClosedUnderHostileRegistrar"
        "HostileRegistrar")
    string(FIND "${source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Terminal real-process menu test is missing required row: ${required}")
    endif()
endforeach()
