# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt-shell-preview
    app/captureenvironment.cpp
    app/captureenvironment.h
    common/catalogpaths.cpp
    common/catalogpaths.h
    app/main.cpp
    app/previewoptions.cpp
    app/previewoptions.h
    app/screenshotcapture.cpp
    app/screenshotcapture.h
    app/shellpreviewapplication.cpp
    app/shellpreviewapplication.h
)

qt_add_qml_module(
    qindaqt-shell-preview
    URI QindaQt.Shell
    VERSION 1.0
    QML_FILES
        qml/AppletChip.qml
        qml/BuiltinAppletContent.qml
        qml/ClockApplet.qml
        qml/NotificationCenterApplet.qml
        qml/ContainerTabStrip.qml
        qml/DecorationButtons.qml
        qml/DesktopPreview.qml
        qml/Main.qml
        qml/MemberWindow.qml
        qml/PanelAppletColumn.qml
        qml/PanelAppletRow.qml
        qml/PanelContent.qml
        qml/PanelSurface.qml
        qml/PreviewControls.qml
        qml/WindowGroupPreview.qml
    DEPENDENCIES
        TARGET qindaqt_shell_power_applet_runtime
)

target_link_libraries(
    qindaqt-shell-preview
    PRIVATE
        qindaqt_profiles
        qindaqt_themes
        QindaQt::ShellPowerAppletRuntime
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

qt_import_qml_plugins(qindaqt-shell-preview)

target_compile_definitions(
    qindaqt-shell-preview
    PRIVATE
        QINDAQT_VERSION="${PROJECT_VERSION}"
        QINDAQT_SOURCE_PROFILE_DIR="${PROJECT_SOURCE_DIR}/data/profiles"
        QINDAQT_SOURCE_THEME_DIR="${PROJECT_SOURCE_DIR}/data/themes"
)

qindaqt_enable_warnings(qindaqt-shell-preview)

install(TARGETS qindaqt-shell-preview RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(QINDAQT_BUILD_PRODUCTION_SHELL)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)

    qt_add_executable(
        qindaqt-shell
        common/catalogpaths.cpp
        common/catalogpaths.h
        runtime/main.cpp
        runtime/globalshortcutregistrar.h
        runtime/compositoroutputauthority.cpp
        runtime/compositoroutputauthority.h
        runtime/kglobalaccelshortcutregistrar.cpp
        runtime/kglobalaccelshortcutregistrar.h
        runtime/notificationcenterappletaccess.cpp
        runtime/notificationcenterappletaccess.h
        runtime/notificationcentershortcut.cpp
        runtime/notificationcentershortcut.h
        runtime/notificationwindowcontroller.cpp
        runtime/notificationwindowcontroller.h
        runtime/notificationquietingsettingsbridge.cpp
        runtime/notificationquietingsettingsbridge.h
        runtime/notificationoutputselector.cpp
        runtime/notificationoutputselector.h
        runtime/powerappletcomposition.cpp
        runtime/powerappletcomposition.h
        runtime/qtcompositoroutputauthority.cpp
        runtime/qtcompositoroutputauthority.h
        runtime/runtimeoptions.cpp
        runtime/runtimeoptions.h
        runtime/runtimepanelwindowfactory.cpp
        runtime/runtimepanelwindowfactory.h
        runtime/shelldevelopmentevidence.cpp
        runtime/shelldevelopmentevidence.h
        runtime/shellruntimeapplication.cpp
        runtime/shellruntimeapplication_development.cpp
        runtime/shellruntimeapplication_surfaces.cpp
        runtime/shellruntimeapplication.h
        runtime/settingsroutelauncher.cpp
        runtime/settingsroutelauncher.h
    )

    qt_add_qml_module(
        qindaqt-shell
        URI QindaQt.Shell.Runtime
        VERSION 1.0
        QML_FILES
            qml/AppletChip.qml
            qml/BuiltinAppletContent.qml
            qml/ClockApplet.qml
            qml/NotificationCenterApplet.qml
            qml/NotificationCard.qml
            qml/NotificationCenter.qml
            qml/NotificationListSection.qml
            qml/NotificationOperationStatus.qml
            qml/NotificationPopupStack.qml
            qml/PanelAppletColumn.qml
            qml/PanelAppletRow.qml
            qml/PanelContent.qml
            qml/RuntimePanel.qml
        DEPENDENCIES
            TARGET qindaqt_shell_power_applet_runtime
    )

    target_link_libraries(
        qindaqt-shell
        PRIVATE
            qindaqt_profiles
            QindaQt::AppletRuntime
            QindaQt::NotificationPresentationClient
            QindaQt::NotificationPresentationModel
            QindaQt::NotificationPresentationPolicy
            QindaQt::SessionLockState
            QindaQt::SettingsClient
            QindaQt::ShellLayout
            QindaQt::ShellOrchestration
            QindaQt::ShellSurface
            QindaQt::ShellVisibilityClient
            QindaQt::ShellPowerAppletRuntime
            qindaqt_themes
            KF6::GlobalAccel
            Qt6::Core
            Qt6::DBus
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
    )

    target_compile_definitions(
        qindaqt-shell
        PRIVATE
            QINDAQT_VERSION="${PROJECT_VERSION}"
            QINDAQT_SOURCE_PROFILE_DIR="${PROJECT_SOURCE_DIR}/data/profiles"
            QINDAQT_SOURCE_THEME_DIR="${PROJECT_SOURCE_DIR}/data/themes"
            QINDAQT_SOURCE_APPLET_DIR="${PROJECT_SOURCE_DIR}/data/applets"
            QINDAQT_SOURCE_APPLET_POLICY="${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
    )

    qindaqt_enable_warnings(qindaqt-shell)
    qt_import_qml_plugins(qindaqt-shell)
    install(TARGETS qindaqt-shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    # A narrow stage lets the Power applet prove that the production binary,
    # compiled QML, manifest, selected profile, policy, and theme relocate
    # together without requiring unrelated first-party applications.
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT PowerAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applets/power.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT PowerAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT PowerAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT PowerAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT PowerAppletRuntime
    )
endif()
