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
