# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(route_root "${SOURCE_ROOT}/src/apps/settings/network")
file(GLOB_RECURSE route_cpp LIST_DIRECTORIES false
     "${route_root}/*.h" "${route_root}/*.cpp")
foreach(source IN LISTS route_cpp)
    file(READ "${source}" content)
    if(content MATCHES "services/network_(service|manager_adapter|qt_transport)/"
       OR content MATCHES "<NetworkManager.h>|QtDBus|QDBus|NMClient|nm_client_|libnm")
        message(FATAL_ERROR
            "Network Settings crossed the public NetworkClient boundary in ${source}")
    endif()
    if(content MATCHES "Q_SLOT|Q_SLOTS")
        message(FATAL_ERROR
            "Network Settings exposed a callable slot outside its closed intent surface in ${source}")
    endif()
    string(REGEX MATCHALL "Q_INVOKABLE[^;]*\\)" invokables "${content}")
    set(allowed_invokables
        "Q_INVOKABLE bool reload()"
        "Q_INVOKABLE bool requestScan()"
        "Q_INVOKABLE bool connectKnownNetwork(const QString &knownNetworkId)"
        "Q_INVOKABLE bool disconnectDevice(const QString &deviceInterface)"
    )
    foreach(invokable IN LISTS invokables)
        string(REGEX REPLACE "[ \t\r\n]+" " " normalized "${invokable}")
        string(STRIP "${normalized}" normalized)
        list(FIND allowed_invokables "${normalized}" allowed_index)
        if(allowed_index EQUAL -1)
            message(FATAL_ERROR
                "Network Settings exposed an invokable outside its closed intent surface in ${source}: ${normalized}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE route_qml LIST_DIRECTORIES false "${route_root}/qml/*.qml")
foreach(source IN LISTS route_qml)
    file(READ "${source}" content)
    if(content MATCHES "TextField|TextInput|TextEdit|TextArea|Password|Passphrase|privateKey|SetRadio|radioSettings")
        message(FATAL_ERROR
            "Network Settings QML gained credential/profile/radio editing in ${source}")
    endif()
endforeach()

set(route_cmake "${route_root}/CMakeLists.txt")
if(EXISTS "${route_cmake}")
    file(READ "${route_cmake}" cmake_content)
    if(cmake_content MATCHES "NetworkService|NetworkManagerAdapter|NetworkQtTransport")
        message(FATAL_ERROR
            "Network Settings domain target reversed the public client boundary")
    endif()
    foreach(required IN ITEMS
            "QindaQt::NetworkClient"
            "qindaqt_settings_network_qml"
            "NetworkPage.qml"
            "COMPONENT SettingsAppearanceRuntime")
        if(NOT cmake_content MATCHES "${required}")
            message(FATAL_ERROR
                "Network Settings package registry is incomplete: ${required}")
        endif()
    endforeach()
endif()

set(settings_main "${SOURCE_ROOT}/src/apps/settings_center/main.cpp")
if(EXISTS "${settings_main}")
    file(READ "${settings_main}" main_content)
    if(main_content MATCHES "services/network_(service|manager_adapter)/|NetworkManager.h|QindaQt::Network(Service|ManagerAdapter)")
        message(FATAL_ERROR
            "Settings composition linked a private Network1 implementation")
    endif()
endif()

list(LENGTH route_cpp cpp_count)
list(LENGTH route_qml qml_count)
if(EXISTS "${route_root}" AND cpp_count EQUAL 0)
    message(FATAL_ERROR "Network Settings boundary found no C++ domain sources")
endif()
message(STATUS
    "Network Settings remains a public-client-only, credential-free route (${cpp_count} C++ and ${qml_count} QML files)")
