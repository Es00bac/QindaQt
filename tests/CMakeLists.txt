# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(core)
add_subdirectory(profiles)
add_subdirectory(themes)
add_subdirectory(tools)

if(QINDAQT_BUILD_SHELL)
    add_subdirectory(shell)
endif()
