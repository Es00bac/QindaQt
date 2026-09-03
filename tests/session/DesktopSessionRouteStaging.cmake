# Settings route module staging for the private desktop component (DesktopVirtual).
# Included by DesktopSessionTests.cmake inside its guarded block so the parent file stays
# within the source-shape budget. Every shared Settings route module the Settings Center
# links must be staged here (Appearance, Display, Network, Audio, Bluetooth, Power,
# Clipboard).

    qt_query_qml_module(
        qindaqt_settings_appearance_qml
        QMLDIR _qindaqt_desktop_appearance_qmldir
        TYPEINFO _qindaqt_desktop_appearance_typeinfo
        QML_FILES _qindaqt_desktop_appearance_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_appearance_deploy_paths
    )
    qt_query_qml_module(
        qindaqt_settings_display_qml
        QMLDIR _qindaqt_desktop_display_qmldir
        TYPEINFO _qindaqt_desktop_display_typeinfo
        QML_FILES _qindaqt_desktop_display_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_display_deploy_paths
    )
    qt_query_qml_module(
        qindaqt_settings_network_qml
        QMLDIR _qindaqt_desktop_network_qmldir
        TYPEINFO _qindaqt_desktop_network_typeinfo
        QML_FILES _qindaqt_desktop_network_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_network_deploy_paths
    )
    qt_query_qml_module(
        qindaqt_settings_audio_qml
        QMLDIR _qindaqt_desktop_audio_qmldir
        TYPEINFO _qindaqt_desktop_audio_typeinfo
        QML_FILES _qindaqt_desktop_audio_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_audio_deploy_paths
    )
    qt_query_qml_module(
        qindaqt_settings_bluetooth_qml
        QMLDIR _qindaqt_desktop_bluetooth_qmldir
        TYPEINFO _qindaqt_desktop_bluetooth_typeinfo
        QML_FILES _qindaqt_desktop_bluetooth_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_bluetooth_deploy_paths
    )
    qt_query_qml_module(
        qindaqt_settings_power_qml
        QMLDIR _qindaqt_desktop_power_qmldir
        TYPEINFO _qindaqt_desktop_power_typeinfo
        QML_FILES _qindaqt_desktop_power_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_power_deploy_paths
    )
    qt_query_qml_module(
        qindaqt_settings_clipboard_qml
        QMLDIR _qindaqt_desktop_clipboard_qmldir
        TYPEINFO _qindaqt_desktop_clipboard_typeinfo
        QML_FILES _qindaqt_desktop_clipboard_qml_files
        QML_FILES_DEPLOY_PATHS _qindaqt_desktop_clipboard_deploy_paths
    )
    install(
        TARGETS
            qindaqt_settings_appearance_qml
            qindaqt_settings_appearance_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_appearance_qmldir}"
            "${_qindaqt_desktop_appearance_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_appearance_qml_files
            _qindaqt_desktop_appearance_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    install(
        TARGETS
            qindaqt_settings_display_qml
            qindaqt_settings_display_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_display_qmldir}"
            "${_qindaqt_desktop_display_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_display_qml_files
            _qindaqt_desktop_display_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Display/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    install(
        TARGETS
            qindaqt_settings_network_qml
            qindaqt_settings_network_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_network_qmldir}"
            "${_qindaqt_desktop_network_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_network_qml_files
            _qindaqt_desktop_network_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Network/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    # AGENT-NOTE: qindaqt-settings links the Audio, Bluetooth, Power, and Clipboard route
    # modules added after this harness was written; the private desktop stage
    # installs only DesktopVirtual, so each shared route module must be staged
    # here like Appearance/Display/Network or the staged Settings Center fails
    # with "module ... is not installed" and desktop readiness times out.
    install(
        TARGETS
            qindaqt_settings_audio_qml
            qindaqt_settings_audio_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_audio_qmldir}"
            "${_qindaqt_desktop_audio_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_audio_qml_files
            _qindaqt_desktop_audio_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Audio/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    install(
        TARGETS
            qindaqt_settings_bluetooth_qml
            qindaqt_settings_bluetooth_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Bluetooth"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Bluetooth"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Bluetooth"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_bluetooth_qmldir}"
            "${_qindaqt_desktop_bluetooth_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Bluetooth"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_bluetooth_qml_files
            _qindaqt_desktop_bluetooth_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Bluetooth/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    install(
        TARGETS
            qindaqt_settings_power_qml
            qindaqt_settings_power_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_power_qmldir}"
            "${_qindaqt_desktop_power_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_power_qml_files
            _qindaqt_desktop_power_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Power/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
    install(
        TARGETS
            qindaqt_settings_clipboard_qml
            qindaqt_settings_clipboard_qmlplugin
        RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
            COMPONENT DesktopVirtual
        LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
            COMPONENT DesktopVirtual
        ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
            COMPONENT DesktopVirtual
    )
    install(
        FILES
            "${_qindaqt_desktop_clipboard_qmldir}"
            "${_qindaqt_desktop_clipboard_typeinfo}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard"
        COMPONENT DesktopVirtual
    )
    foreach(qml_file deploy_path IN ZIP_LISTS
            _qindaqt_desktop_clipboard_qml_files
            _qindaqt_desktop_clipboard_deploy_paths)
        cmake_path(GET deploy_path PARENT_PATH deploy_directory)
        cmake_path(GET deploy_path FILENAME deploy_name)
        install(
            FILES "${qml_file}"
            DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Clipboard/${deploy_directory}"
            RENAME "${deploy_name}"
            COMPONENT DesktopVirtual
        )
    endforeach()
