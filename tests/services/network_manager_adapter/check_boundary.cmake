# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(service_root "${SOURCE_ROOT}/src/services/network_service")
set(transport_root "${SOURCE_ROOT}/src/services/network_qt_transport")
set(adapter_root "${SOURCE_ROOT}/src/services/network_manager_adapter")

file(GLOB_RECURSE service_sources LIST_DIRECTORIES false
     "${service_root}/*.h" "${service_root}/*.cpp" "${service_root}/CMakeLists.txt")
foreach(source IN LISTS service_sources)
    file(READ "${source}" content)
    if(content MATCHES "<NetworkManager.h>|nm_client_|PkgConfig::NetworkManager")
        message(FATAL_ERROR "Resident Network1 service imports NetworkManager in ${source}")
    endif()
    if(content MATCHES "services/network_(qt_transport|manager_adapter|client)/")
        message(FATAL_ERROR "Resident Network1 service reverses module direction in ${source}")
    endif()
endforeach()

file(GLOB_RECURSE transport_sources LIST_DIRECTORIES false
     "${transport_root}/*.h" "${transport_root}/*.cpp" "${transport_root}/CMakeLists.txt")
foreach(source IN LISTS transport_sources)
    file(READ "${source}" content)
    if(content MATCHES "<NetworkManager.h>|nm_client_|libnm|services/network_service/|services/network_manager_adapter/")
        message(FATAL_ERROR "Qt Network transport crosses service/platform boundary in ${source}")
    endif()
endforeach()

file(GLOB_RECURSE adapter_sources LIST_DIRECTORIES false
     "${adapter_root}/*.h" "${adapter_root}/*.cpp")
foreach(source IN LISTS adapter_sources)
    file(READ "${source}" content)
    if(content MATCHES "nm_remote_connection_get_secrets|nm_remote_connection_get_secrets_async"
       OR content MATCHES "nm_setting_[a-z0-9_]+_get_(psk|password|password_raw|private_key|private_key_password|client_cert)"
       OR content MATCHES "NM_SETTING_WIRELESS_SECURITY_PSK|NM_SETTING_802_1X_(PASSWORD|PRIVATE_KEY)"
       OR content MATCHES "QProcess|nmcli|system\\(")
        message(FATAL_ERROR "Forbidden secret/process escape in NetworkManager adapter ${source}")
    endif()
endforeach()

file(GLOB_RECURSE public_adapter_headers LIST_DIRECTORIES false
     "${adapter_root}/include/*.h")
foreach(source IN LISTS public_adapter_headers)
    file(READ "${source}" content)
    if(content MATCHES "<NetworkManager.h>|NMClient|NMDevice|GDBusConnection")
        message(FATAL_ERROR "Public NetworkManager adapter header leaks platform handles in ${source}")
    endif()
endforeach()

set(interface_xml "${adapter_root}/data/org.qindaqt.Network1.xml")
if(EXISTS "${interface_xml}")
    file(READ "${interface_xml}" interface_content)
    if(interface_content MATCHES "a\\{sv\\}")
        message(FATAL_ERROR "Network1 fixed wire was replaced by a variant bag")
    endif()
endif()

set(adapter_cmake "${adapter_root}/CMakeLists.txt")
if(EXISTS "${adapter_cmake}")
    file(READ "${adapter_cmake}" adapter_cmake_content)
    if(NOT adapter_cmake_content MATCHES "COMPONENT QindaQtNetworkN1"
       OR NOT adapter_cmake_content MATCHES "qindaqt-network-service"
       OR NOT adapter_cmake_content MATCHES "org.qindaqt.Network1.xml")
        message(FATAL_ERROR "Network N1 package registry is incomplete")
    endif()
endif()

set(unit "${adapter_root}/data/qindaqt-network-service.service.in")
if(EXISTS "${unit}")
    file(READ "${unit}" unit_content)
    if(unit_content MATCHES "AF_INET|AF_NETLINK")
        message(FATAL_ERROR "Network1 user service gained direct network/socket authority")
    endif()
    if(NOT unit_content MATCHES "Type=dbus"
       OR NOT unit_content MATCHES "BusName=org.qindaqt.Network1"
       OR NOT unit_content MATCHES "Restart=on-failure"
       OR NOT unit_content MATCHES "RestrictAddressFamilies=AF_UNIX")
        message(FATAL_ERROR "Network1 user service residency profile is incomplete")
    endif()
endif()

message(STATUS "Network N1 keeps service, exact-owner transport, libnm, and secrets separated")
