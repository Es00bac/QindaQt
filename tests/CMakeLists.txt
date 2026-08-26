# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(core)
add_subdirectory(profiles)
add_subdirectory(themes)
add_subdirectory(tools)
add_subdirectory(applets)
if(TARGET qindaqt_applet_host)
    add_subdirectory(applet_host)
endif()
add_subdirectory(settings)

if(QINDAQT_BUILD_SHELL)
    add_subdirectory(shell)
endif()
