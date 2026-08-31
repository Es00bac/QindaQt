# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED STAGE_INCLUDE_DIR)
    message(FATAL_ERROR "Missing installed Display runtime include root")
endif()

set(runtime_dir "${STAGE_INCLUDE_DIR}/qindaqt/services/display_runtime")
file(GLOB installed_headers RELATIVE "${runtime_dir}" "${runtime_dir}/*.h")
list(SORT installed_headers)
set(expected_headers resident_display_runtime.h session_safety_port.h state_root.h)
list(SORT expected_headers)
if(NOT installed_headers STREQUAL expected_headers)
    message(FATAL_ERROR
        "Installed Display runtime public split changed: ${installed_headers}")
endif()
foreach(header IN LISTS installed_headers)
    file(READ "${runtime_dir}/${header}" content)
    if(content MATCHES "session_lock_state" OR content MATCHES "QtWayland"
       OR content MATCHES "wayland-client" OR content MATCHES "FileJournalStore"
       OR content MATCHES "KWin/" OR content MATCHES "KScreen")
        message(FATAL_ERROR "Private dependency leaked through installed ${header}")
    endif()
endforeach()

message(STATUS "Installed Display runtime contains only public composition seams")
