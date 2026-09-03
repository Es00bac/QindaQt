# SPDX-License-Identifier: GPL-3.0-or-later

if(DEFINED SCAN_ROOT)
    get_filename_component(route_root "${SCAN_ROOT}" ABSOLUTE)
    file(GLOB_RECURSE route_files LIST_DIRECTORIES false
         "${route_root}/*.cpp" "${route_root}/*.h" "${route_root}/*.qml")
elseif(DEFINED SOURCE_ROOT)
    get_filename_component(
        route_root "${SOURCE_ROOT}/src/apps/settings/bluetooth" ABSOLUTE)
    file(GLOB_RECURSE route_files LIST_DIRECTORIES false
         "${route_root}/*.cpp" "${route_root}/*.h" "${route_root}/*.qml")
else()
    message(FATAL_ERROR "SOURCE_ROOT or SCAN_ROOT is required")
endif()
file(REAL_PATH "${route_root}" route_root)

get_filename_component(repository_root "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
file(GLOB_RECURSE repository_include_candidates LIST_DIRECTORIES false
     "${repository_root}/src/*")
set(allowed_public_include_prefixes
    "qindaqt/apps/settings_bluetooth/"
    "qindaqt/services/bluetooth_client/"
    "qindaqt/services/bluetooth_protocol/"
)

foreach(source IN LISTS route_files)
    file(READ "${source}" contents)
    if(contents MATCHES "bluetooth_(service|model|bluez_adapter)/|BluezQt|QtDBus|QDBus")
        message(FATAL_ERROR
            "Bluetooth Settings crossed its public-client-only boundary in ${source}")
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
                "Bluetooth Settings imported a non-public repository header '${include_path}' in ${source}")
        endif()
    endforeach()
    if(contents MATCHES "Q_INVOKABLE[^\n]*(pair|trust|remove)" OR
       contents MATCHES "Q_INVOKABLE[^\n]*(Pair|Trust|Remove)")
        message(FATAL_ERROR
            "Bluetooth Settings gained forbidden pairing/trust/remove authority in ${source}")
    endif()
endforeach()

if(DEFINED SOURCE_ROOT)
    file(READ "${route_root}/CMakeLists.txt" cmake_contents)
    foreach(required IN ITEMS "QindaQt::BluetoothClient"
            "qindaqt_settings_bluetooth_qml" "BluetoothPage.qml"
            "COMPONENT SettingsAppearanceRuntime")
        if(NOT cmake_contents MATCHES "${required}")
            message(FATAL_ERROR "Bluetooth Settings package registry is incomplete: ${required}")
        endif()
    endforeach()
endif()
list(LENGTH route_files file_count)
if(file_count EQUAL 0)
    message(FATAL_ERROR "Bluetooth Settings boundary found no route files")
endif()
message(STATUS "Bluetooth Settings public-client allow-list accepted ${file_count} files")
