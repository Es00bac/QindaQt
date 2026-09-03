# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(agent_root "${SOURCE_ROOT}/src/services/network_secret_agent")
file(GLOB_RECURSE agent_sources LIST_DIRECTORIES false
     "${agent_root}/*.h" "${agent_root}/*.cpp" "${agent_root}/*.qml")
foreach(source IN LISTS agent_sources)
    file(READ "${source}" content)
    if(content MATCHES
       "qindaqt/(services/network_(protocol|client|service|manager_adapter|qt_transport)|apps/settings)|src/(shell|compositor)")
        message(FATAL_ERROR
            "Network secret agent crossed its confined process boundary in ${source}")
    endif()
    if(content MATCHES "QSettings|QSaveFile|QFile[ \\t]*[<(]|qDebug|qInfo|qWarning|qCritical")
        message(FATAL_ERROR
            "Network secret agent gained persistence or payload-capable logging in ${source}")
    endif()
endforeach()

# AGENT-NOTE: Raman P1-3 found f06d2fd's sole diagnostic canary installed an
# empty handler and then scanned test-authored text. Keep the registered
# boundary row hostile to that exact vacuous proof shape.
set(controller_test
    "${SOURCE_ROOT}/tests/services/network_secret_agent/tst_secret_agent_controller.cpp")
file(READ "${controller_test}" controller_test_content)
if(controller_test_content MATCHES
   "qInstallMessageHandler[^;]*\\{[ \\t\\r\\n]*\\}[^;]*;")
    message(FATAL_ERROR
        "Network secret-agent diagnostics proof discards the captured channel")
endif()

# AGENT-NOTE: Raman P3-1 was a proof-precision finding. The focused prompt row
# must retain explicit navigation/state and Enter/window-close cases.
set(prompt_test
    "${SOURCE_ROOT}/tests/services/network_secret_agent/tst_network_secret_prompt.cpp")
file(READ "${prompt_test}" prompt_test_content)
foreach(required_proof
        "traversesControlsAndExposesCheckboxStates"
        "enterSubmitsAndWindowCloseCancels")
    if(NOT prompt_test_content MATCHES "${required_proof}")
        message(FATAL_ERROR
            "Network secret-agent prompt proof lost ${required_proof}")
    endif()
endforeach()

# P1-1 also applies to standard secret-bearing inputs, not just reply values.
set(agent_object "${agent_root}/src/secret_agent_object.cpp")
file(READ "${agent_object}" agent_object_content)
string(REGEX MATCHALL "wipeSettingsMap\\(connection\\)" input_wipes
       "${agent_object_content}")
list(LENGTH input_wipes input_wipe_count)
if(input_wipe_count LESS 3)
    message(FATAL_ERROR
        "Network secret-agent does not scrub every standard secret-bearing input")
endif()

set(network1_roots
    "${SOURCE_ROOT}/src/services/network_protocol"
    "${SOURCE_ROOT}/src/services/network_client"
    "${SOURCE_ROOT}/src/services/network_service"
    "${SOURCE_ROOT}/src/services/network_manager_adapter"
    "${SOURCE_ROOT}/src/services/network_qt_transport")
foreach(root IN LISTS network1_roots)
    if(EXISTS "${root}")
        file(GLOB_RECURSE network1_sources LIST_DIRECTORIES false
             "${root}/*.h" "${root}/*.cpp" "${root}/CMakeLists.txt")
        foreach(source IN LISTS network1_sources)
            file(READ "${source}" content)
            if(content MATCHES "network_secret_agent|NetworkSecretAgent")
                message(FATAL_ERROR
                    "Network1 gained a dependency on the secret agent in ${source}")
            endif()
        endforeach()
    endif()
endforeach()

list(LENGTH agent_sources source_count)
if(EXISTS "${agent_root}" AND source_count EQUAL 0)
    message(FATAL_ERROR "Network secret-agent boundary found no product sources")
endif()
message(STATUS
    "Network secret agent remains process-confined and outside Network1 (${source_count} sources)")
