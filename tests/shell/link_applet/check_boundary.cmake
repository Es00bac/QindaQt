# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0435: the QindaLink applet is a public Link1 consumer (phone.read plus
# phone.control). Its sources may not reach the D-Bus transport or the link
# service; may not start processes (Open QindaLink goes through the injected
# launcher seam); and its QML may not contain any text-entry control.
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing SOURCE_ROOT")
endif()

set(applet_root "${SOURCE_ROOT}/src/shell/link_applet")
file(GLOB_RECURSE cpp_sources "${applet_root}/*.cpp" "${applet_root}/*.h")
file(GLOB qml_sources "${applet_root}/qml/*.qml")
if(cpp_sources STREQUAL "" OR qml_sources STREQUAL "")
    message(FATAL_ERROR "Link applet sources were not found under ${applet_root}")
endif()

set(forbidden_cpp
    "QDBus" "QtDBus" "qt_link_transport" "QtLinkTransport" "link_service" "QProcess" "startDetached")
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

# Every action goes through the typed LinkClient methods; nothing may forward
# a raw D-Bus call.
file(READ "${applet_root}/src/link_applet_controller.cpp" controller)
foreach(required IN ITEMS "m_client->confirmPairing(" "m_client->ring(" "m_client->unpair(")
    string(FIND "${controller}" "${required}" hit)
    if(hit EQUAL -1)
        message(FATAL_ERROR "link_applet_controller.cpp lacks typed call ${required}")
    endif()
endforeach()
foreach(token IN ITEMS "requestOperation" "sendSms" "input" "RemoteDesktop")
    string(FIND "${controller}" "${token}" hit)
    if(NOT hit EQUAL -1)
        message(FATAL_ERROR "link_applet_controller.cpp must not use '${token}'")
    endif()
endforeach()
