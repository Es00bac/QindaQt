# SPDX-License-Identifier: GPL-3.0-or-later

# Launcher declares its runtime after early source dependencies exist. At this
# point the complete source graph is available, so materialize it before the
# preview and production QML modules name it.
qindaqt_add_launcher_runtime()

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
        TARGET qindaqt_shell_launcher_qml
        TARGET qindaqt_global_menu_qml
        TARGET qindaqt_shell_audio_applet_runtime
        TARGET qindaqt_shell_bluetooth_applet_runtime
        TARGET qindaqt_shell_clipboard_applet_runtime
        TARGET qindaqt_shell_power_applet_runtime
)

target_link_libraries(
    qindaqt-shell-preview
    PRIVATE
        qindaqt_profiles
        qindaqt_themes
        QindaQt::ShellAudioAppletRuntime
        QindaQt::ShellBluetoothAppletRuntime
        QindaQt::ShellClipboardAppletRuntime
        QindaQt::ShellLauncherQml
        QindaQt::ShellPowerAppletRuntime
        QindaQt::GlobalMenuQml
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

    add_library(
        qindaqt_shell_panel_visibility_producers STATIC
        runtime/panelvisibilityanimation.cpp
        runtime/panelvisibilityanimation.h
        runtime/panelvisibilitypointer.cpp
        runtime/panelvisibilitypointer.h
        runtime/panelvisibilitypopup.cpp
        runtime/panelvisibilitypopup.h
        runtime/panelvisibilityruntime.cpp
        runtime/panelvisibilityruntime.h
        runtime/panelvisibilityshortcut.cpp
        runtime/panelvisibilityshortcut.h
        runtime/panelvisibilitytimer.cpp
        runtime/panelvisibilitytimer.h
    )
    target_include_directories(
        qindaqt_shell_panel_visibility_producers PUBLIC runtime
    )
    target_link_libraries(
        qindaqt_shell_panel_visibility_producers
        PUBLIC
            QindaQt::SettingsClient
            QindaQt::ShellOrchestration
            QindaQt::ShellSurface
            Qt6::Core
            Qt6::Gui
            Qt6::Quick
    )
    set_target_properties(
        qindaqt_shell_panel_visibility_producers
        PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF
    )
    qindaqt_enable_warnings(qindaqt_shell_panel_visibility_producers)

    qt_add_executable(
        qindaqt-shell
        common/catalogpaths.cpp
        common/catalogpaths.h
        runtime/main.cpp
        runtime/audioappletcomposition.cpp
        runtime/audioappletcomposition.h
        runtime/bluetoothappletcomposition.cpp
        runtime/bluetoothappletcomposition.h
        runtime/clipboardappletcomposition.cpp
        runtime/clipboardappletcomposition.h
        runtime/globalshortcutregistrar.h
        runtime/compositoroutputauthority.cpp
        runtime/compositoroutputauthority.h
        runtime/globalmenuappletcomposition.cpp
        runtime/globalmenuappletcomposition.h
        runtime/kglobalaccelshortcutregistrar.cpp
        runtime/kglobalaccelshortcutregistrar.h
        runtime/launcherappletcomposition.cpp
        runtime/launcherappletcomposition.h
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
            TARGET qindaqt_shell_launcher_qml
            TARGET qindaqt_global_menu_qml
            TARGET qindaqt_shell_audio_applet_runtime
            TARGET qindaqt_shell_bluetooth_applet_runtime
            TARGET qindaqt_shell_clipboard_applet_runtime
            TARGET qindaqt_shell_power_applet_runtime
    )

    target_link_libraries(
        qindaqt-shell
        PRIVATE
            qindaqt_profiles
            QindaQt::AppletRuntime
            QindaQt::ClipboardClient
            QindaQt::NotificationPresentationClient
            QindaQt::NotificationPresentationModel
            QindaQt::NotificationPresentationPolicy
            QindaQt::SessionLockState
            QindaQt::SettingsClient
            QindaQt::ShellLayout
            QindaQt::ShellOrchestration
            QindaQt::ShellSurface
            QindaQt::ShellVisibilityClient
            QindaQt::ShellWindowActionsClient
            QindaQt::GlobalMenuApplet
            QindaQt::GlobalMenuQml
            QindaQt::GlobalMenuRegistrar
            QindaQt::GlobalMenuTransportComposition
            QindaQt::ShellAudioAppletRuntime
            QindaQt::ShellBluetoothAppletRuntime
            QindaQt::ShellClipboardAppletRuntime
            QindaQt::ShellLauncherQml
            QindaQt::ShellLauncherRuntime
            QindaQt::ShellPowerAppletRuntime
            QindaQt::SessionActions
            qindaqt_shell_panel_visibility_producers
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
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT QindaQt
    )
    # AGENT-CONTRACT: The default component is a runnable shell package, just
    # like each narrow applet component below. Keep the shell's directly linked
    # Controls library and Controls' Tokens dependency in their relative
    # $ORIGIN layouts or a component-filtered install fails before main().
    install(
        TARGETS qindaqt_controls_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT QindaQt
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT QindaQt
    )
    install(
        TARGETS qindaqt_shell_launcher_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT QindaQt
    )
    install(
        TARGETS qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT QindaQt
    )
    # A narrow stage proves that the Bluetooth applet's production binary,
    # compiled QML, manifest, selected profile, policy, and theme relocate as
    # one package boundary.
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT BluetoothAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applets/bluetooth.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT BluetoothAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT BluetoothAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT BluetoothAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT BluetoothAppletRuntime
    )
    # AGENT-CONTRACT: Every narrow component that stages qindaqt-shell must
    # carry its directly linked Controls library and the Tokens library at
    # Controls' baked $ORIGIN/../Tokens sibling path. Otherwise the staged
    # shell fails before its applet catalog can be inspected.
    install(
        TARGETS qindaqt_controls_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT BluetoothAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT BluetoothAppletRuntime
    )
    install(
        TARGETS qindaqt_shell_launcher_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT BluetoothAppletRuntime
    )
    install(
        TARGETS qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT BluetoothAppletRuntime
    )
    # A narrow stage lets the Audio applet prove that the production binary,
    # compiled QML, manifest, selected profile, policy, and theme relocate
    # together without requiring unrelated first-party applications.
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT AudioAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applets/audio.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT AudioAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT AudioAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT AudioAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT AudioAppletRuntime
    )
    # The audio applet renders through the shared QindaQt.Controls and
    # QindaQt.Tokens QML modules. Controls carries a baked
    # $ORIGIN/../Tokens RUNPATH (the sibling-module layout built by
    # src/controls), so this component must preserve that runtime closure.
    install(
        TARGETS qindaqt_controls_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT AudioAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT AudioAppletRuntime
    )
    install(
        TARGETS qindaqt_shell_launcher_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT AudioAppletRuntime
    )
    install(
        TARGETS qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT AudioAppletRuntime
    )
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
    install(
        TARGETS qindaqt_controls_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT PowerAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT PowerAppletRuntime
    )
    install(
        TARGETS qindaqt_shell_launcher_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT PowerAppletRuntime
    )
    install(
        TARGETS qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT PowerAppletRuntime
    )

    # The launcher component proves the whole production boundary rather than
    # only a standalone QML import: shell, manifest/profile/policy/theme,
    # compiled launcher module, and the executable's loader closure.
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT LauncherAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applets/launcher.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT LauncherAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT LauncherAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT LauncherAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT LauncherAppletRuntime
    )
    install(
        TARGETS qindaqt_controls_qml qindaqt_shell_launcher_qml
                qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT LauncherAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT LauncherAppletRuntime
    )

    # Global Menu's narrow component carries the live shell, compiled module,
    # registrar policy inputs, and the complete direct loader closure.
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT GlobalMenuAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applets/global-menu.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT GlobalMenuAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT GlobalMenuAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT GlobalMenuAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT GlobalMenuAppletRuntime
    )
    install(
        TARGETS qindaqt_controls_qml qindaqt_shell_launcher_qml
                qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT GlobalMenuAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT GlobalMenuAppletRuntime
    )
    # Clipboard's narrow production component proves the live shell, audited
    # manifest inputs, compiled applet module, and loader closure relocate as
    # one runnable package boundary.
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT ClipboardAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applets/clipboard.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT ClipboardAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT ClipboardAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT ClipboardAppletRuntime
    )
    install(
        FILES "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT ClipboardAppletRuntime
    )
    install(
        TARGETS qindaqt_controls_qml qindaqt_shell_launcher_qml
                qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT ClipboardAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT ClipboardAppletRuntime
    )

    # A narrow stage proves that the task-list applet's production binary,
    # compiled QML, manifest, selected profile, policy, and theme relocate as
    # one package boundary.
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applets/task-list.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/profiles/qindaqt.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/profiles"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/themes/qinda-dark.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
        COMPONENT TaskListAppletRuntime
    )
    install(
        FILES
            "${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applet-policy"
        COMPONENT TaskListAppletRuntime
    )
    # AGENT-CONTRACT: Every narrow component that stages qindaqt-shell must
    # carry its directly linked Controls library and the Tokens library at
    # Controls' baked $ORIGIN/../Tokens sibling path. Otherwise the staged
    # shell fails before its applet catalog can be inspected.
    install(
        TARGETS qindaqt_controls_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT TaskListAppletRuntime
    )
    install(
        TARGETS qindaqt_tokens_qml
        LIBRARY DESTINATION "Tokens"
        COMPONENT TaskListAppletRuntime
    )
    install(
        TARGETS qindaqt_shell_launcher_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT TaskListAppletRuntime
    )
    install(
        TARGETS qindaqt_global_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT TaskListAppletRuntime
    )

    # AGENT-CONTRACT: BuiltinAppletContent imports ClipboardApplet directly,
    # so every component that stages qindaqt-shell must stage that module too.
    foreach(shell_component IN ITEMS
            QindaQt AudioAppletRuntime BluetoothAppletRuntime
            ClipboardAppletRuntime GlobalMenuAppletRuntime
            LauncherAppletRuntime PowerAppletRuntime TaskListAppletRuntime)
        qindaqt_install_clipboard_applet_runtime("${shell_component}")
    endforeach()
endif()

# The Clipboard applet's packaged component carries its own manifest, mirroring
# the PowerAppletRuntime precedent above. The applet module itself
# (src/shell/clipboard_applet) owns the compiled-library/QML install rules.
install(
    FILES
        "${PROJECT_SOURCE_DIR}/data/applets/clipboard.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
    COMPONENT ClipboardApplet
)
