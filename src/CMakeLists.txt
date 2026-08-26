# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(profiles)
add_subdirectory(shell_layout)
add_subdirectory(themes)
add_subdirectory(core)
add_subdirectory(hybrid)
add_subdirectory(hybrid_constraints)
add_subdirectory(hybrid_chrome)
add_subdirectory(hybrid_input)
if(QINDAQT_BUILD_KWIN_PLUGIN)
    # The decoration is part of the binary KWin integration contract. Keeping
    # it behind the same option preserves the documented dependency-free
    # compositor-bridge configuration.
    add_subdirectory(decorations)
endif()
add_subdirectory(compositor)
add_subdirectory(applets)
add_subdirectory(applet_host)
add_subdirectory(session)
add_subdirectory(settings)

if(QINDAQT_BUILD_SHELL)
    add_subdirectory(shell)
endif()
