# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED STAGE_INCLUDE_DIR)
    message(FATAL_ERROR "Missing installed Display writer include root")
endif()

set(writer_dir "${STAGE_INCLUDE_DIR}/qindaqt/services/display_writer")
file(GLOB installed_headers RELATIVE "${writer_dir}" "${writer_dir}/*.h")
list(SORT installed_headers)
set(expected_headers
    output_management_port.h
    production_output_management_port.h
    writer_transaction_port.h
)
list(SORT expected_headers)
if(NOT installed_headers STREQUAL expected_headers)
    message(FATAL_ERROR
        "Installed Display writer public/private split changed: ${installed_headers}")
endif()

foreach(header IN LISTS installed_headers)
    file(READ "${writer_dir}/${header}" content)
    if(content MATCHES "QtWayland" OR content MATCHES "wayland-client"
       OR content MATCHES "qwayland-" OR content MATCHES "kde-output-.*xml")
        message(FATAL_ERROR "Private protocol dependency leaked through ${header}")
    endif()
endforeach()

file(GLOB_RECURSE installed_protocol_files
     "${STAGE_INCLUDE_DIR}/*kde-output*" "${STAGE_INCLUDE_DIR}/*.xml")
if(installed_protocol_files)
    message(FATAL_ERROR
        "Private Display writer protocol files were installed: ${installed_protocol_files}")
endif()

message(STATUS "Installed Display writer boundary contains only public headers")
