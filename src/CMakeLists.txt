# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(profiles)
add_subdirectory(themes)
add_subdirectory(core)
add_subdirectory(compositor)
add_subdirectory(applets)
add_subdirectory(applet_host)
add_subdirectory(session)
add_subdirectory(settings)

if(QINDAQT_BUILD_SHELL)
    add_subdirectory(shell)
endif()
