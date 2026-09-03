# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(protocol)
add_subdirectory(ownership)
add_subdirectory(exporter)
add_subdirectory(qt_widgets_adapter)
add_subdirectory(applet)
add_subdirectory(composition)
add_subdirectory(registrar)
add_subdirectory(dbusmenu)
add_subdirectory(transport_composition)
add_subdirectory(runtime_composition)
add_subdirectory(boundary)
add_subdirectory(qml)

if(TARGET qindaqt-shell)
    add_test(
        NAME qindaqt.global-menu-installed-package
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
            "-DQINDAQT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DQINDAQT_STAGE=${CMAKE_CURRENT_BINARY_DIR}/installed-global-menu"
            "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
            "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
            "-DQINDAQT_INSTALL_QMLDIR=${QT6_INSTALL_QML}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_global_menu.cmake"
    )
    set_tests_properties(
        qindaqt.global-menu-installed-package
        PROPERTIES LABELS "shell;global-menu;runtime;installed;poison" TIMEOUT 60
    )
endif()
