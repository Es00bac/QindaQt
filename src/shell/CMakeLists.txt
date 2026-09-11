# SPDX-License-Identifier: GPL-3.0-or-later

include(TaskListRuntimeInstall.cmake)
include(StatusNotifierRuntimeInstall.cmake)
include(DesktopControlsRuntimeInstall.cmake)
include(IconRuntimeInstall.cmake)
include(ShellAppletRuntimeInstall.cmake)

find_package(Qt6 6.11 REQUIRED COMPONENTS Svg)

# Launcher declares its runtime after early source dependencies exist. At this
# point the complete source graph is available, so materialize it before the
# preview and production QML modules name it.
qindaqt_add_launcher_runtime()

qt_add_executable(
    qindaqt-shell-preview
    app/captureenvironment.cpp
    app/captureenvironment.h
    common/catalogpaths.cpp
    common/catalogpaths.h common/shelliconconfiguration.cpp common/shelliconconfiguration.h
    common/shelltokenpublisher.cpp
    common/shelltokenpublisher.h
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
        qml/DesktopControlsAppletComponents.qml
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
        TARGET qindaqt_shell_task_list_applet
        TARGET qindaqt_shell_status_notifier_applet_runtime TARGET qindaqt_shell_icons
        TARGET qindaqt_shell_desktop_controls_runtime
        TARGET qindaqt_start_menu_qml
)

target_link_libraries(
    qindaqt-shell-preview
    PRIVATE
        qindaqt_profiles
        qindaqt_themes
        QindaQt::TokensQml
        QindaQt::ShellAudioAppletRuntime
        QindaQt::ShellBluetoothAppletRuntime
        QindaQt::ShellClipboardAppletRuntime
        QindaQt::ShellLauncherQml
        QindaQt::ShellPowerAppletRuntime
        QindaQt::ShellTaskListApplet
        QindaQt::ShellStatusNotifierAppletRuntime QindaQt::ShellIcons
        QindaQt::ShellDesktopControlsRuntime
        QindaQt::ShellStartMenuQml
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

# ShellTokenPublisher gives preview the production shell's direct QST loader
# dependency; keep it relocatable in the full-install sibling Tokens layout.
set_property(TARGET qindaqt-shell-preview APPEND PROPERTY INSTALL_RPATH "$ORIGIN/../Tokens")
install(TARGETS qindaqt-shell-preview RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(QINDAQT_BUILD_PRODUCTION_SHELL)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
    find_package(LayerShellQt 6.6.6 REQUIRED)

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
        runtime/runtime_layout_adoption.cpp
        runtime/runtime_layout_adoption.h
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
            QindaQt::AppAppearance
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
        common/catalogpaths.h common/shelliconconfiguration.cpp common/shelliconconfiguration.h common/shelltokenpublisher.cpp
        common/shelltokenpublisher.h
        runtime/main.cpp
        runtime/audioappletcomposition.cpp
        runtime/audioappletcomposition.h
        runtime/bluetoothappletcomposition.cpp
        runtime/bluetoothappletcomposition.h
        runtime/clipboardappletcomposition.cpp
        runtime/clipboardappletcomposition.h
        runtime/desktopcontrolscomposition.cpp
        runtime/desktopcontrolscomposition.h
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
        runtime/runtimepanelappletcompatibility.cpp
        runtime/runtimepanelappletcompatibility.h
        runtime/runtimepanelwindowfactory.cpp
        runtime/runtimepanelwindowfactory.h
        runtime/shelldevelopmentevidence.cpp
        runtime/shelldevelopmentevidence.h
        runtime/shellruntimeapplication.cpp
        runtime/shellruntimeapplication_applets.cpp
        runtime/shellruntimeapplication_development.cpp
        runtime/shellruntimeapplication_surfaces.cpp
        runtime/shellruntimeapplication_tokens.cpp
        runtime/shellruntimeapplication_wallpaper.cpp
        runtime/shellruntimeapplication.h
        runtime/settingsroutelauncher.cpp
        runtime/settingsroutelauncher.h
        runtime/shellappearancebridge.cpp
        runtime/shellappearancebridge.h
        runtime/shellpreferencevalues.cpp
        runtime/shellpreferencevalues.h
        runtime/shellstartuppreferences.cpp
        runtime/shellstartuppreferences.h
        runtime/shortcutnotecontroller.cpp
        runtime/shortcutnotecontroller.h
        runtime/shortcutnoteshortcut.cpp
        runtime/shortcutnoteshortcut.h
        runtime/wallpapercontroller.cpp
        runtime/wallpapercontroller.h
        runtime/tasklistappletcomposition.cpp
        runtime/taskorderpersistence.cpp
        runtime/panelquickconfig.cpp
        runtime/tasklistappletcomposition.h
        runtime/statusnotifierappletcomposition.cpp
        runtime/statusnotifierappletcomposition.h
    )

    qt_add_qml_module(
        qindaqt-shell
        URI QindaQt.Shell.Runtime
        VERSION 1.0
        QML_FILES
            qml/AppletChip.qml
            qml/BuiltinAppletContent.qml
            qml/DesktopControlsAppletComponents.qml
            qml/ClockApplet.qml
            qml/NotificationCenterApplet.qml
            qml/NotificationCard.qml
            qml/NotificationCenter.qml
            qml/NotificationListSection.qml
            qml/NotificationButton.qml
            qml/NotificationOperationStatus.qml
            qml/NotificationPopupStack.qml
            qml/NotificationToolButton.qml
            qml/PanelAppletColumn.qml
            qml/PanelAppletRow.qml
            qml/PanelContent.qml
            qml/ShortcutNoteCard.qml
            qml/RuntimePanel.qml
        DEPENDENCIES
            TARGET qindaqt_shell_launcher_qml
            TARGET qindaqt_global_menu_qml
            TARGET qindaqt_shell_audio_applet_runtime
            TARGET qindaqt_shell_bluetooth_applet_runtime
            TARGET qindaqt_shell_clipboard_applet_runtime
            TARGET qindaqt_shell_power_applet_runtime
            TARGET qindaqt_shell_task_list_applet
            TARGET qindaqt_shell_status_notifier_applet_runtime TARGET qindaqt_shell_icons
            TARGET qindaqt_shell_desktop_controls_runtime
            TARGET qindaqt_start_menu_qml
            TARGET qindaqt_desktop_surface_qml
    )

    target_link_libraries(
        qindaqt-shell
        PRIVATE
            qindaqt_profiles
            QindaQt::TokensQml
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
            QindaQt::ShellTaskListApplet
            QindaQt::ShellTaskListProducer
            QindaQt::ShellTaskListOperations
            QindaQt::ShellStatusNotifierAppletRuntime QindaQt::ShellIcons
            QindaQt::ShellDesktopControlsRuntime
            QindaQt::ShellStartMenuQml
            QindaQt::ShellDesktopSurface
            QindaQt::ShellDesktopSurfaceQml
            # AGENT-CONTRACT: the desktop surface is reached only from C++
            # (QQmlComponent::loadFromModule), never from an `import` in
            # the shell's QML files, so Qt's import scan cannot discover
            # its static plugin. Link the plugin target explicitly (as the
            # desktop-surface tests do) or the installed shell reports
            # `No module named "QindaQt.Shell.DesktopSurface" found`.
            qindaqt_desktop_surface_qmlplugin
            QindaQt::ShellWorkspacesDBus
            QindaQt::StatusNotifierWatcher
            QindaQt::SessionActions
            QindaQt::PanelBlur
            qindaqt_shell_panel_visibility_producers
            qindaqt_themes
            KF6::GlobalAccel
            Qt6::Core
            Qt6::DBus
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
            LayerShellQt::Interface
    )

    target_compile_definitions(
        qindaqt-shell
        PRIVATE
            QINDAQT_VERSION="${PROJECT_VERSION}"
            QINDAQT_SOURCE_PROFILE_DIR="${PROJECT_SOURCE_DIR}/data/profiles"
            QINDAQT_SOURCE_THEME_DIR="${PROJECT_SOURCE_DIR}/data/themes"
            QINDAQT_SOURCE_APPLET_DIR="${PROJECT_SOURCE_DIR}/data/applets"
            QINDAQT_SOURCE_APPLET_POLICY="${PROJECT_SOURCE_DIR}/data/applet-policy/default.json"
            QINDAQT_SHELL_BUILD_EXECUTABLE_PATH="${CMAKE_BINARY_DIR}/src/shell/qindaqt-shell${CMAKE_EXECUTABLE_SUFFIX}"
    )

    qindaqt_enable_warnings(qindaqt-shell)
    qt_import_qml_plugins(qindaqt-shell)
    # TokenFacade is implemented by the process-local QML backing library.
    # AGENT-GUARD: narrow shell components install that library at the existing
    # sibling Tokens path; the executable must resolve it before main() can
    # publish QST-1, without ambient loader variables.
    set_property(
        TARGET qindaqt-shell APPEND PROPERTY INSTALL_RPATH
        "$ORIGIN/../Tokens"
    )
    install(
        TARGETS qindaqt-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT QindaQt
    )
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/data/wallpapers/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/wallpapers" COMPONENT QindaQt)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/data/icons/QindaQt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons" COMPONENT QindaQt)
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
    install(
        TARGETS qindaqt_start_menu_qml
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT QindaQt
    )
    qindaqt_install_shell_primary_applet_components()

    qindaqt_install_task_list_shell_runtime()
    qindaqt_install_status_notifier_shell_runtime()

    # AGENT-CONTRACT: stage BuiltinAppletContent's direct imports with qindaqt-shell.
    foreach(shell_component IN ITEMS
            QindaQt AudioAppletRuntime BluetoothAppletRuntime
            ClipboardAppletRuntime GlobalMenuAppletRuntime
            LauncherAppletRuntime PowerAppletRuntime TaskListAppletRuntime
            StatusNotifierAppletRuntime)
        qindaqt_install_clipboard_applet_runtime("${shell_component}")
        qindaqt_install_task_list_applet_runtime("${shell_component}")
        qindaqt_install_status_notifier_applet_runtime("${shell_component}")
        qindaqt_install_shell_icons_runtime("${shell_component}")
        qindaqt_install_desktop_controls_applet_runtime("${shell_component}")
        qindaqt_install_start_menu_applet_runtime("${shell_component}")
        qindaqt_install_desktop_surface_runtime("${shell_component}")
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
