# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED STAGE_INCLUDE_DIR)
    message(FATAL_ERROR "Missing installed Display journal include root")
endif()

set(journal_dir "${STAGE_INCLUDE_DIR}/qindaqt/services/display_journal")
file(GLOB installed_headers RELATIVE "${journal_dir}" "${journal_dir}/*.h")
if(NOT installed_headers STREQUAL "file_journal_store.h")
    message(FATAL_ERROR
        "Installed Display journal public/private split changed: ${installed_headers}")
endif()
file(READ "${journal_dir}/file_journal_store.h" content)
if(content MATCHES "fcntl.h|sys/stat.h|unistd.h|QFile|openat|renameat")
    message(FATAL_ERROR "Filesystem implementation leaked through installed header")
endif()

message(STATUS "Installed Display journal boundary contains one platform-free header")
