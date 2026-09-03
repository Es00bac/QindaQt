# SPDX-License-Identifier: GPL-3.0-or-later

if(DEFINED SCAN_ROOT)
    file(GLOB_RECURSE customize_boundary_files
         "${SCAN_ROOT}/*.cpp" "${SCAN_ROOT}/*.h" "${SCAN_ROOT}/*.qml")
elseif(DEFINED SOURCE_ROOT)
    set(customize_root "${SOURCE_ROOT}/src/apps/settings/customize")
    set(customize_boundary_files
        "${customize_root}/customize_catalog.cpp"
        "${customize_root}/customize_catalog.h"
        "${customize_root}/customize_editor_host.cpp"
        "${customize_root}/customize_settings_actions.cpp"
        "${customize_root}/customize_settings_model.cpp"
        "${customize_root}/customize_settings_projection.cpp"
        "${customize_root}/include/qindaqt/apps/settings_customize/customize_editor_host.h"
        "${customize_root}/include/qindaqt/apps/settings_customize/customize_settings_model.h")
else()
    message(FATAL_ERROR "SOURCE_ROOT or SCAN_ROOT is required")
endif()

set(forbidden_pattern
    "LayerShellQt|shell_surface|shell/runtime|shell/qml|QDBusConnection|KWin/|kwin\\.h|wayland-server")
foreach(source IN LISTS customize_boundary_files)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "missing Customize boundary source: ${source}")
    endif()
    file(READ "${source}" contents)
    if(contents MATCHES "${forbidden_pattern}")
        message(FATAL_ERROR
            "Customize editor core crossed a shell/compositor/session-bus boundary: ${source}")
    endif()
endforeach()
