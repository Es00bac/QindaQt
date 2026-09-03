# SPDX-License-Identifier: GPL-3.0-or-later

file(GLOB_RECURSE production_files
    "${SOURCE_ROOT}/src/services/clipboard_protocol/*"
    "${SOURCE_ROOT}/src/services/clipboard_client/*"
    "${SOURCE_ROOT}/src/services/clipboard_service/*"
    "${SOURCE_ROOT}/src/services/clipboard_wayland_adapter/*")
foreach(path IN LISTS production_files)
    if(NOT IS_DIRECTORY "${path}")
        file(READ "${path}" contents)
        if(contents MATCHES "QFile|QSaveFile|QStandardPaths|std::ofstream|fopen\\(")
            message(FATAL_ERROR "clipboard production boundary gained persistence: ${path}")
        endif()
        if(contents MATCHES "qDebug\\(|qInfo\\(")
            message(FATAL_ERROR "clipboard production boundary gained payload-risk logging: ${path}")
        endif()
    endif()
endforeach()
