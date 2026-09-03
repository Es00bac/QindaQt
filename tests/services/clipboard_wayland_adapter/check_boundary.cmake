# SPDX-License-Identifier: GPL-3.0-or-later

file(SHA256 "${MODULE_SOURCE_DIR}/protocol/ext-data-control-v1.xml" actual_sha)
if(NOT actual_sha STREQUAL EXPECTED_XML_SHA256)
    message(FATAL_ERROR "ext-data-control-v1 XML checksum drift: ${actual_sha}")
endif()
file(GLOB_RECURSE public_headers "${MODULE_SOURCE_DIR}/include/*")
foreach(header IN LISTS public_headers)
    file(READ "${header}" contents)
    if(contents MATCHES "wayland-|wl_display|QtWayland")
        message(FATAL_ERROR "Wayland implementation escaped public header: ${header}")
    endif()
endforeach()
