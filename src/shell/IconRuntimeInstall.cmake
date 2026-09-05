# SPDX-License-Identifier: GPL-3.0-or-later

# Stage the static Icons module with any component whose QML imports it. The
# public icon module owns its sources and target; this helper owns only the
# install-component closure needed by production applets and shell probes.
function(qindaqt_install_shell_icons_runtime component_name)
    if(NOT TARGET qindaqt_shell_icons OR
       NOT TARGET qindaqt_shell_iconsplugin)
        message(FATAL_ERROR
                "Shell Icons targets must exist before staging their runtime")
    endif()

    qt_query_qml_module(
        qindaqt_shell_icons
        QMLDIR icons_qmldir
        TYPEINFO icons_typeinfo
        QML_FILES icons_qml_files
        QML_FILES_DEPLOY_PATHS icons_deploy_paths
    )
    install(
        TARGETS qindaqt_shell_icons qindaqt_shell_iconsplugin
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Icons"
            COMPONENT "${component_name}"
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Icons"
            COMPONENT "${component_name}"
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Icons"
            COMPONENT "${component_name}"
    )
    install(
        FILES "${icons_qmldir}" "${icons_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Shell/Icons"
        COMPONENT "${component_name}"
    )
    foreach(qml_file deploy_path IN ZIP_LISTS icons_qml_files icons_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION
                "${QT6_INSTALL_QML}/QindaQt/Shell/Icons/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT "${component_name}"
        )
    endforeach()
endfunction()
