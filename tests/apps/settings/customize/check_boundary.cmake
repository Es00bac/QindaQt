# SPDX-License-Identifier: GPL-3.0-or-later

if(DEFINED SCAN_ROOT)
    get_filename_component(customize_root "${SCAN_ROOT}" ABSOLUTE)
    file(GLOB_RECURSE customize_boundary_files
         "${SCAN_ROOT}/*.cpp" "${SCAN_ROOT}/*.h" "${SCAN_ROOT}/*.qml")
elseif(DEFINED SOURCE_ROOT)
    get_filename_component(
        customize_root "${SOURCE_ROOT}/src/apps/settings/customize" ABSOLUTE)
    file(GLOB_RECURSE customize_boundary_files LIST_DIRECTORIES false
         "${customize_root}/*.cpp" "${customize_root}/*.h"
         "${customize_root}/*.qml")
else()
    message(FATAL_ERROR "SOURCE_ROOT or SCAN_ROOT is required")
endif()
file(REAL_PATH "${customize_root}" customize_root)

get_filename_component(
    repository_root "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
file(GLOB_RECURSE repository_include_candidates LIST_DIRECTORIES false
     "${repository_root}/src/*")

set(forbidden_pattern
    "LayerShellQt|shell_surface|shell/runtime|shell/qml|KWin/|kwin\\.h|wayland-server")
# AGENT-CONTRACT: This list mirrors the Customize route's documented public
# dependencies. A new prefix requires the same-change boundary documentation.
set(allowed_public_include_prefixes
    "qindaqt/apps/settings_customize/"
    "qindaqt/applets/"
    "qindaqt/design_tokens/"
    "qindaqt/profiles/"
    "qindaqt/services/settings_client/"
    "qindaqt/services/settings_protocol/"
    "qindaqt/shell_customization/"
    "qindaqt/shell_customization_editor/"
    "qindaqt/shell_layout/"
    "qindaqt/themes/"
)
foreach(source IN LISTS customize_boundary_files)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "missing Customize boundary source: ${source}")
    endif()
    file(READ "${source}" contents)
    if(contents MATCHES "${forbidden_pattern}")
        message(FATAL_ERROR
            "Customize editor core crossed a shell/compositor/session-bus boundary: ${source}")
    endif()
    string(REGEX MATCHALL
        "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]"
        include_directives "${contents}")
    foreach(include_directive IN LISTS include_directives)
        string(REGEX REPLACE
            "^[^<\"]*[<\"]([^>\"]+)[>\"].*$" "\\1"
            include_path "${include_directive}")
        set(include_allowed FALSE)
        set(is_repository_path FALSE)
        set(is_route_local_path FALSE)
        if(include_path MATCHES "(^|/)\\.\\.(/|$)"
           OR include_path MATCHES "(^|/)src/"
           OR include_path MATCHES "^(tests|docs|data|cmake|tools)/")
            set(is_repository_path TRUE)
        endif()

        if(NOT include_path MATCHES "(^|/)\\.\\.(/|$)")
            get_filename_component(source_directory "${source}" DIRECTORY)
            set(local_candidate "${source_directory}/${include_path}")
            if(EXISTS "${local_candidate}")
                file(REAL_PATH "${local_candidate}" local_candidate)
                string(FIND "${local_candidate}" "${customize_root}/"
                    route_prefix_position)
                if(route_prefix_position EQUAL 0)
                    set(is_route_local_path TRUE)
                endif()
            endif()
        endif()

        if(NOT is_route_local_path AND NOT include_path MATCHES "^qindaqt/")
            set(include_suffix "/${include_path}")
            string(LENGTH "${include_suffix}" include_suffix_length)
            foreach(candidate IN LISTS repository_include_candidates)
                string(LENGTH "${candidate}" candidate_length)
                if(candidate_length LESS include_suffix_length)
                    continue()
                endif()
                math(EXPR expected_suffix_position
                    "${candidate_length} - ${include_suffix_length}")
                string(FIND
                    "${candidate}" "${include_suffix}" suffix_position REVERSE)
                if(suffix_position EQUAL expected_suffix_position)
                    set(is_repository_path TRUE)
                    break()
                endif()
            endforeach()
        endif()

        if(NOT is_repository_path AND include_path MATCHES "^qindaqt/")
            foreach(prefix IN LISTS allowed_public_include_prefixes)
                string(FIND "${include_path}" "${prefix}" prefix_position)
                if(prefix_position EQUAL 0)
                    set(include_allowed TRUE)
                    break()
                endif()
            endforeach()
        elseif(NOT is_repository_path AND is_route_local_path)
            set(include_allowed TRUE)
        elseif(NOT is_repository_path
               AND NOT include_directive MATCHES
                   "#[ \t]*include[ \t]*\"")
            set(include_allowed TRUE)
        endif()

        if(NOT include_allowed)
            message(FATAL_ERROR
                "Customize editor imported a non-public repository header '${include_path}' in ${source}")
        endif()
    endforeach()
    if(contents MATCHES "QDBusConnection"
       AND NOT source STREQUAL
           "${customize_root}/customize_route_composition.cpp")
        message(FATAL_ERROR
            "Customize session-bus composition escaped its sole adapter: ${source}")
    endif()
endforeach()
