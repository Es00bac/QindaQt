# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(profiles)
add_subdirectory(themes)
add_subdirectory(core)

if(QINDAQT_BUILD_SHELL)
    add_subdirectory(shell)
endif()
