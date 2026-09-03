# SPDX-License-Identifier: GPL-3.0-or-later

if(DEFINED SCAN_ROOT)
    file(GLOB_RECURSE customize_boundary_files
         "${SCAN_ROOT}/*.cpp" "${SCAN_ROOT}/*.h" "${SCAN_ROOT}/*.qml")
elseif(DEFINED SOURCE_ROOT)
    set(customize_root "${SOURCE_ROOT}/src/apps/settings/customize")
    file(GLOB_RECURSE customize_boundary_files LIST_DIRECTORIES false
         "${customize_root}/*.cpp" "${customize_root}/*.h"
         "${customize_root}/*.qml")
else()
    message(FATAL_ERROR "SOURCE_ROOT or SCAN_ROOT is required")
endif()

set(forbidden_pattern
    "LayerShellQt|shell_surface|shell/runtime|shell/qml|KWin/|kwin\\.h|wayland-server")
set(private_module_path_pattern
    "#[ \t]*include[ \t]*[<\"][^>\"\r\n]*src/(shell_customization|shell_customization_editor|profiles)/")
set(private_module_subdir_pattern
    "#[ \t]*include[ \t]*[<\"][^>\"\r\n]*(shell_customization|shell_customization_editor|profiles)/(src|include)/")
set(private_header_pattern
    "#[ \t]*include[ \t]*[<\"][^>\"\r\n]*_p\\.h[>\"]")
set(private_src_path_pattern
    "#[ \t]*include[ \t]*[<\"][^>\"\r\n]*src/[^>\"\r\n]+/src/[^>\"\r\n]*[>\"]")
foreach(source IN LISTS customize_boundary_files)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "missing Customize boundary source: ${source}")
    endif()
    file(READ "${source}" contents)
    if(contents MATCHES "${forbidden_pattern}")
        message(FATAL_ERROR
            "Customize editor core crossed a shell/compositor/session-bus boundary: ${source}")
    endif()
    if(contents MATCHES "${private_module_path_pattern}"
       OR contents MATCHES "${private_module_subdir_pattern}"
       OR contents MATCHES "${private_header_pattern}"
       OR contents MATCHES "${private_src_path_pattern}")
        message(FATAL_ERROR
            "Customize editor imported a private repository header: ${source}")
    endif()
    if(contents MATCHES "QDBusConnection"
       AND NOT source STREQUAL
           "${customize_root}/customize_route_composition.cpp")
        message(FATAL_ERROR
            "Customize session-bus composition escaped its sole adapter: ${source}")
    endif()
endforeach()
