# SPDX-License-Identifier: GPL-3.0-or-later

if(DEFINED SCAN_ROOT)
    get_filename_component(route_root "${SCAN_ROOT}" ABSOLUTE)
elseif(DEFINED SOURCE_ROOT)
    get_filename_component(
        route_root "${SOURCE_ROOT}/src/apps/settings/clipboard" ABSOLUTE)
else()
    message(FATAL_ERROR "SOURCE_ROOT or SCAN_ROOT is required")
endif()
file(GLOB_RECURSE route_files LIST_DIRECTORIES false
     "${route_root}/*.cpp" "${route_root}/*.h" "${route_root}/*.qml")
file(REAL_PATH "${route_root}" route_root)

get_filename_component(repository_root "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
file(GLOB_RECURSE repository_include_candidates LIST_DIRECTORIES false
     "${repository_root}/src/*")
set(allowed_public_include_prefixes
    "qindaqt/apps/settings_clipboard/"
    "qindaqt/services/clipboard_client/"
    "qindaqt/services/clipboard_model/clipboard_descriptor.h"
    "qindaqt/services/clipboard_protocol/clipboard_protocol.h"
    "qindaqt/services/settings_client/"
    "qindaqt/services/settings_protocol/settings_wire_status.h"
)

foreach(source IN LISTS route_files)
    file(READ "${source}" contents)
    if(contents MATCHES "clipboard_(service|wayland_adapter)/|QClipboard|QMimeData")
        message(FATAL_ERROR
            "Clipboard Settings crossed its public-client-only boundary in ${source}")
    endif()
    string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]"
           include_directives "${contents}")
    foreach(include_directive IN LISTS include_directives)
        string(REGEX REPLACE "^[^<\"]*[<\"]([^>\"]+)[>\"].*$" "\\1"
               include_path "${include_directive}")
        set(include_allowed FALSE)
        set(repository_path FALSE)
        set(route_local FALSE)
        if(include_path MATCHES "(^|/)\\.\\.(/|$)" OR include_path MATCHES "(^|/)src/"
           OR include_path MATCHES "^(tests|docs|data|cmake|tools)/")
            set(repository_path TRUE)
        endif()
        if(NOT include_path MATCHES "(^|/)\\.\\.(/|$)")
            get_filename_component(source_directory "${source}" DIRECTORY)
            set(local_candidate "${source_directory}/${include_path}")
            if(EXISTS "${local_candidate}")
                file(REAL_PATH "${local_candidate}" local_candidate)
                string(FIND "${local_candidate}" "${route_root}/" local_position)
                if(local_position EQUAL 0)
                    set(route_local TRUE)
                endif()
            endif()
        endif()
        if(NOT route_local AND NOT include_path MATCHES "^qindaqt/")
            set(include_suffix "/${include_path}")
            foreach(candidate IN LISTS repository_include_candidates)
                string(FIND "${candidate}" "${include_suffix}" suffix_position REVERSE)
                if(NOT suffix_position EQUAL -1)
                    set(repository_path TRUE)
                endif()
            endforeach()
        endif()
        if(NOT repository_path AND include_path MATCHES "^qindaqt/")
            foreach(prefix IN LISTS allowed_public_include_prefixes)
                string(FIND "${include_path}" "${prefix}" prefix_position)
                if(prefix_position EQUAL 0)
                    set(include_allowed TRUE)
                endif()
            endforeach()
        elseif(NOT repository_path AND route_local)
            set(include_allowed TRUE)
        elseif(NOT repository_path AND NOT include_directive MATCHES "#[ \t]*include[ \t]*\"")
            set(include_allowed TRUE)
        endif()
        if(NOT include_allowed)
            message(FATAL_ERROR
                "Clipboard Settings imported a non-public repository header '${include_path}' in ${source}")
        endif()
    endforeach()

    if(source MATCHES "\\.(cpp|h)$" AND
       contents MATCHES "decodeValue|ClipboardValue|ClipboardFormat|Q_INVOKABLE[^\n]*(copy|select|remove|delete|paste|read)|m_clipboardClient\\.(copy|select|remove)\\(")
        message(FATAL_ERROR
            "Clipboard Settings gained forbidden content-read or per-entry authority in ${source}")
    endif()
    if(source MATCHES "\\.qml$" AND
       contents MATCHES "clipboardSettings\\.(copy|select|remove|delete|paste|read)[A-Za-z]*\\(")
        message(FATAL_ERROR
            "Clipboard Settings QML gained forbidden content authority in ${source}")
    endif()
    if((contents MATCHES "QtDBus|QDBus") AND
       NOT source MATCHES "/clipboard_route_composition\\.cpp$")
        message(FATAL_ERROR
            "Clipboard Settings leaked D-Bus outside its route composition root in ${source}")
    endif()
endforeach()

if(DEFINED SOURCE_ROOT)
    file(READ "${route_root}/CMakeLists.txt" cmake_contents)
    foreach(required IN ITEMS "QindaQt::ClipboardClient" "QindaQt::SettingsClient"
            "qindaqt_settings_clipboard_qml" "ClipboardPage.qml"
            "COMPONENT SettingsAppearanceRuntime")
        if(NOT cmake_contents MATCHES "${required}")
            message(FATAL_ERROR
                "Clipboard Settings package registry is incomplete: ${required}")
        endif()
    endforeach()
endif()
list(LENGTH route_files file_count)
if(file_count EQUAL 0)
    message(FATAL_ERROR "Clipboard Settings boundary found no route files")
endif()
message(STATUS "Clipboard Settings public-client allow-list accepted ${file_count} files")
