# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0258 / ADR-0069: the Network applet is a public Network1 consumer with
# no credential surface. Its sources may not reach the D-Bus transport, the
# resident service, the NetworkManager adapter, or the secret agent; may not
# start processes (the Settings route is injected by shell composition); and
# its QML may not contain any text-entry control that could collect a secret.
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing SOURCE_ROOT")
endif()

set(applet_root "${SOURCE_ROOT}/src/shell/network_applet")
file(GLOB_RECURSE cpp_sources "${applet_root}/*.cpp" "${applet_root}/*.h")
file(GLOB qml_sources "${applet_root}/qml/*.qml")
if(cpp_sources STREQUAL "" OR qml_sources STREQUAL "")
    message(FATAL_ERROR "Network applet sources were not found under ${applet_root}")
endif()

set(forbidden_cpp
    "QDBus" "QtDBus" "qt_network_transport" "network_service" "network_manager"
    "NetworkManager" "libnm" "network_secret_agent" "QProcess" "startDetached"
    "passphrase" "psk" "Psk" "PSK")
set(forbidden_qml
    "TextField" "TextInput" "TextEdit" "echoMode" "passphrase" "import QtDBus"
    "Qt.createQmlObject")

foreach(source IN LISTS cpp_sources)
    file(READ "${source}" contents)
    foreach(token IN LISTS forbidden_cpp)
        string(FIND "${contents}" "${token}" hit)
        if(NOT hit EQUAL -1)
            message(FATAL_ERROR "${source} must not reference '${token}'")
        endif()
    endforeach()
endforeach()

foreach(source IN LISTS qml_sources)
    file(READ "${source}" contents)
    foreach(token IN LISTS forbidden_qml)
        string(FIND "${contents}" "${token}" hit)
        if(NOT hit EQUAL -1)
            message(FATAL_ERROR "${source} must not contain '${token}'")
        endif()
    endforeach()
endforeach()

# Every mutation goes through the typed NetworkClient intents; nothing may
# forward raw operation kinds or parameter maps.
file(READ "${applet_root}/src/network_applet_actions.cpp" actions)
foreach(required IN ITEMS "m_client->setRadio(" "m_client->connectKnownNetwork("
                          "m_client->connectVisibleNetwork(" "m_client->disconnectDevice("
                          "m_client->requestScan(")
    string(FIND "${actions}" "${required}" hit)
    if(hit EQUAL -1)
        message(FATAL_ERROR "network_applet_actions.cpp lacks typed intent ${required}")
    endif()
endforeach()
foreach(token IN ITEMS "requestOperation" "QVariantMap")
    string(FIND "${actions}" "${token}" hit)
    if(NOT hit EQUAL -1)
        message(FATAL_ERROR "network_applet_actions.cpp must not use '${token}'")
    endif()
endforeach()
