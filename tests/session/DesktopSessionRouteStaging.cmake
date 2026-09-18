# Settings route module staging for the private desktop component (DesktopVirtual).
# Included by DesktopSessionTests.cmake inside its guarded block so the parent file stays
# within the source-shape budget.
#
# AGENT-NOTE: qindaqt-settings links every shared Settings route module, and the private
# desktop stage installs only DesktopVirtual, so each module is staged here with its
# plugin, qmldir, type information, and QML files. A module missing from this list makes
# the staged Settings Center fail with "module ... is not installed", desktop readiness
# time out, and desktop.virtual.stage-closure report the module without a qmldir.

    foreach(_qindaqt_desktop_route IN ITEMS
            Appearance Display Network Audio Bluetooth Power Clipboard Color
            Accessibility Input Streaming DateTime Windows
            # Name:target-suffix -- this module's CMake target does not
            # match its URI, so the suffix is spelled out rather than derived.
            DefaultApplications:default_apps AboutComputer:about_computer)
        string(REPLACE ":" ";" _qindaqt_desktop_route_parts "${_qindaqt_desktop_route}")
        list(GET _qindaqt_desktop_route_parts 0 _qindaqt_desktop_route_name)
        list(LENGTH _qindaqt_desktop_route_parts _qindaqt_desktop_route_partcount)
        if(_qindaqt_desktop_route_partcount GREATER 1)
            list(GET _qindaqt_desktop_route_parts 1 _qindaqt_desktop_route_id)
        else()
            string(TOLOWER "${_qindaqt_desktop_route_name}" _qindaqt_desktop_route_id)
        endif()
        set(_qindaqt_desktop_route_target "qindaqt_settings_${_qindaqt_desktop_route_id}_qml")
        set(_qindaqt_desktop_route_destination
            "${QT6_INSTALL_QML}/QindaQt/SettingsApp/${_qindaqt_desktop_route_name}")
        qt_query_qml_module(
            ${_qindaqt_desktop_route_target}
            QMLDIR _qindaqt_desktop_route_qmldir
            TYPEINFO _qindaqt_desktop_route_typeinfo
            QML_FILES _qindaqt_desktop_route_qml_files
            QML_FILES_DEPLOY_PATHS _qindaqt_desktop_route_deploy_paths
        )
        install(
            TARGETS
                ${_qindaqt_desktop_route_target}
                ${_qindaqt_desktop_route_target}plugin
            RUNTIME DESTINATION "${_qindaqt_desktop_route_destination}"
                COMPONENT DesktopVirtual
            LIBRARY DESTINATION "${_qindaqt_desktop_route_destination}"
                COMPONENT DesktopVirtual
            ARCHIVE DESTINATION "${_qindaqt_desktop_route_destination}"
                COMPONENT DesktopVirtual
        )
        install(
            FILES
                "${_qindaqt_desktop_route_qmldir}"
                "${_qindaqt_desktop_route_typeinfo}"
            DESTINATION "${_qindaqt_desktop_route_destination}"
            COMPONENT DesktopVirtual
        )
        foreach(qml_file deploy_path IN ZIP_LISTS
                _qindaqt_desktop_route_qml_files
                _qindaqt_desktop_route_deploy_paths)
            cmake_path(GET deploy_path PARENT_PATH deploy_directory)
            cmake_path(GET deploy_path FILENAME deploy_name)
            install(
                FILES "${qml_file}"
                DESTINATION "${_qindaqt_desktop_route_destination}/${deploy_directory}"
                RENAME "${deploy_name}"
                COMPONENT DesktopVirtual
            )
        endforeach()
    endforeach()
