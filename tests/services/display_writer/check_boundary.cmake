# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS SOURCE_ROOT EXPECTED_DEVICE_SHA256 EXPECTED_MANAGEMENT_SHA256)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Display writer boundary input: ${required}")
    endif()
endforeach()

set(module "${SOURCE_ROOT}/src/services/display_writer")
file(GLOB_RECURSE public_headers LIST_DIRECTORIES false
     "${module}/include/*.h" "${module}/include/*.hh"
     "${module}/include/*.hpp" "${module}/include/*.hxx")
foreach(header IN LISTS public_headers)
    file(READ "${header}" content)
    if(content MATCHES "QtWayland" OR content MATCHES "wayland-client"
       OR content MATCHES "KWin/" OR content MATCHES "KScreen")
        message(FATAL_ERROR "Platform dependency escaped public boundary: ${header}")
    endif()
endforeach()

file(GLOB_RECURSE sources LIST_DIRECTORIES false
     "${module}/*.h" "${module}/*.hh" "${module}/*.hpp" "${module}/*.hxx"
     "${module}/*.cc" "${module}/*.cpp" "${module}/*.cxx")
foreach(source IN LISTS sources)
    file(READ "${source}" content)
    if(content MATCHES "#include[ ]*[<\"]kwin" OR content MATCHES "KScreen::"
       OR content MATCHES "kscreen-doctor" OR content MATCHES "QtDBus")
        message(FATAL_ERROR "Forbidden private/oracle dependency in ${source}")
    endif()
endforeach()

file(SHA256 "${module}/protocol/kde-output-device-v2.xml" device_sha256)
file(SHA256 "${module}/protocol/kde-output-management-v2.xml" management_sha256)
if(NOT device_sha256 STREQUAL EXPECTED_DEVICE_SHA256)
    message(FATAL_ERROR "Pinned kde-output-device-v2.xml checksum changed")
endif()
if(NOT management_sha256 STREQUAL EXPECTED_MANAGEMENT_SHA256)
    message(FATAL_ERROR "Pinned kde-output-management-v2.xml checksum changed")
endif()

message(STATUS "Display writer public/private boundary and protocol checksums verified")
