# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0431: the Firewall applet is a public Firewall1 consumer with
# `security.read` only; it never changes the firewall. Its sources may not
# reach the D-Bus transport or the QindaFirewall service itself; may not
# start processes; and its QML may not contain any text-entry control.
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing SOURCE_ROOT")
endif()

set(applet_root "${SOURCE_ROOT}/src/shell/firewall_applet")
file(GLOB_RECURSE cpp_sources "${applet_root}/*.cpp" "${applet_root}/*.h")
file(GLOB qml_sources "${applet_root}/qml/*.qml")
if(cpp_sources STREQUAL "" OR qml_sources STREQUAL "")
    message(FATAL_ERROR "Firewall applet sources were not found under ${applet_root}")
endif()

set(forbidden_cpp
    "QDBus" "QtDBus" "qt_firewall_transport" "QProcess" "startDetached")
set(forbidden_qml
    "TextField" "TextInput" "TextEdit" "echoMode" "import QtDBus" "Qt.createQmlObject")

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

# Every action goes through the typed FirewallClient methods; nothing may
# forward a raw D-Bus call or an invented control verb.
file(READ "${applet_root}/src/firewall_applet_controller.cpp" controller)
foreach(required IN ITEMS "m_client->openWindow(" "m_client->dismiss(" "m_client->confirmMine(")
    string(FIND "${controller}" "${required}" hit)
    if(hit EQUAL -1)
        message(FATAL_ERROR "firewall_applet_controller.cpp lacks typed call ${required}")
    endif()
endforeach()
foreach(token IN ITEMS "requestOperation" "setRadio" "block" "Block" "autoblock")
    string(FIND "${controller}" "${token}" hit)
    if(NOT hit EQUAL -1)
        message(FATAL_ERROR "firewall_applet_controller.cpp must not use '${token}': the applet never changes the firewall")
    endif()
endforeach()
