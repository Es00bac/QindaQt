// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "runtimeoptions.h"
#include "clipboardappletcomposition.h"
#include "statusnotifierappletcomposition.h"
#include "../common/shelliconconfiguration.h"

#include "qindaqt/applet_host/capability_policy.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

#include <memory>
#include <optional>

class QGuiApplication;
class QScreen;

namespace QindaQt::ShellSurface {
class LayerShellSurfaceBackend;
class PanelSurfaceController;
}

namespace QindaQt::ShellOrchestration {
class PanelInteractionStore;
}

namespace QindaQt::ShellVisibilityClient {
class CompositorVisibilityClient;
class QtCompositorVisibilityTransport;
}

namespace QindaQt::ShellWindowActionsClient {
class QtShellWindowActionsTransport;
class ShellWindowActionsClient;
}

namespace QindaQt::Services::NotificationPresentationClient {
class NotificationPresentationClient;
class QtNotificationPresentationTransport;
}

namespace QindaQt::Services::NotificationPresentationModel {
class NotificationPresentationController;
}

namespace QindaQt::Services::NotificationPresentationPolicy {
class NotificationInterruptionPolicy;
class NotificationPrivacyPolicy;
}
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
class QtSettingsTransport;
}

namespace QindaQt::Services::SessionLockState {
class QtSessionLockTransport;
class SessionLockStateMonitor;
}

namespace QindaQt::Shell {

class RuntimePanelWindowFactory;
class AudioAppletComposition;
class BluetoothAppletComposition;
class GlobalMenuAppletComposition;
class KGlobalAccelShortcutRegistrar;
class LauncherAppletComposition;
class NotificationCenterAppletAccess;
class NotificationCenterShortcut;
class NotificationWindowController;
class NotificationQuietingSettingsBridge;
class PanelVisibilityRuntime;
class PowerAppletComposition;
class QtCompositorOutputAuthority;
class ShellDevelopmentEvidence;
class SettingsRouteLauncher;
class ShellTokenPublisher;
class TaskListAppletComposition;

class ShellRuntimeApplication final : public QObject {
    Q_OBJECT

public:
    explicit ShellRuntimeApplication(QGuiApplication &application);
    ~ShellRuntimeApplication() override;

    int run();

private:
    [[nodiscard]] bool loadCatalogs(const RuntimeOptions &options, QString *error);
    [[nodiscard]] bool loadPresentationToken(const RuntimeOptions &options,
                                             QString *error);
    void printCatalog() const;
    [[nodiscard]] bool initializeRuntime(const RuntimeOptions &options,
                                         QString *error);
    [[nodiscard]] bool initializeTokens(QString *error);
    [[nodiscard]] bool initializeIcons(QString *error);
    [[nodiscard]] bool initializeLauncherRuntime(QString *error);
    void initializeServiceAppletCompositions();
    void startSettingsClients();
    void restartWindowActionsIdentity();
    void initializePanelVisibility(const Profiles::LayoutProfile &profile);
    [[nodiscard]] bool startDevelopmentEvidence(const RuntimeOptions &options,
                                                QString *error);
    [[nodiscard]] bool reconcileSurfaces(QString *error);
    [[nodiscard]] bool settlePanelVisibility(QString *error);
    void startNotificationOutputAuthority();
    void attachOutputSignals(QScreen *screen);
    void scheduleOutputReconcile();
    void resetRuntime();

    QGuiApplication &m_application;
    Profiles::ProfileCatalog m_profiles;
    Themes::ThemeCatalog m_themes;
    Applets::ManifestCatalog m_applets;
    AppletHost::CapabilityPolicy m_appletPolicy;
    QQmlEngine m_engine;
    ShellDataRoots m_dataRoots;
    std::unique_ptr<ShellTokenPublisher> m_tokenPublisher;
    std::unique_ptr<RuntimePanelWindowFactory> m_windowFactory;
    std::unique_ptr<ShellSurface::LayerShellSurfaceBackend> m_backend;
    std::unique_ptr<ShellSurface::PanelSurfaceController> m_controller;
    std::unique_ptr<ShellVisibilityClient::QtCompositorVisibilityTransport>
        m_visibilityTransport;
    std::unique_ptr<ShellVisibilityClient::CompositorVisibilityClient>
        m_visibilityClient;
    std::unique_ptr<ShellWindowActionsClient::QtShellWindowActionsTransport>
        m_windowActionsTransport;
    std::unique_ptr<ShellWindowActionsClient::ShellWindowActionsClient>
        m_windowActionsClient;
    std::unique_ptr<QtCompositorOutputAuthority> m_outputAuthority;
    std::unique_ptr<ShellOrchestration::PanelInteractionStore> m_interactions;
    std::unique_ptr<KGlobalAccelShortcutRegistrar>
        m_panelVisibilityShortcutRegistrar;
    std::unique_ptr<PanelVisibilityRuntime> m_panelVisibility;
    std::optional<Services::NotificationPresentation::PresentationAccessToken>
        m_presentationAccessToken;
    std::unique_ptr<
        Services::NotificationPresentationClient::QtNotificationPresentationTransport>
        m_notificationTransport;
    std::unique_ptr<
        Services::NotificationPresentationClient::NotificationPresentationClient>
        m_notificationClient;
    // AGENT-GUARD: declaration order makes each borrowed dependency outlive
    // its consumer even if a future teardown bypasses resetRuntime. The lock
    // transport precedes its monitor; the client and both policies precede the
    // presentation controller.
    std::unique_ptr<Services::SessionLockState::QtSessionLockTransport>
        m_sessionLockTransport;
    std::unique_ptr<Services::SessionLockState::SessionLockStateMonitor>
        m_sessionLockMonitor;
    std::unique_ptr<Services::NotificationPresentationPolicy::
                        NotificationInterruptionPolicy>
        m_notificationInterruptionPolicy;
    std::unique_ptr<Services::NotificationPresentationPolicy::
                        NotificationPrivacyPolicy>
        m_notificationPrivacyPolicy;
    std::unique_ptr<Services::SettingsClient::QtSettingsTransport>
        m_settingsTransport;
    std::unique_ptr<Services::SettingsClient::SettingsClient> m_settingsClient;
    std::unique_ptr<Services::SettingsClient::QtSettingsTransport>
        m_quietingSettingsTransport;
    std::unique_ptr<Services::SettingsClient::SettingsClient>
        m_quietingSettingsClient;
    std::unique_ptr<NotificationQuietingSettingsBridge> m_quietingSettingsBridge;
    std::unique_ptr<SettingsRouteLauncher> m_settingsRouteLauncher;
    std::unique_ptr<Services::NotificationPresentationModel::
                        NotificationPresentationController>
        m_notificationPresentation;
    std::unique_ptr<NotificationCenterAppletAccess> m_notificationCenterAccess;
    // AGENT-GUARD: teardown safety comes from ~ShellRuntimeApplication()
    // unconditionally calling resetRuntime(), which tears the window factory
    // down before the applet compositions. Declaration order alone does NOT
    // protect these members: they are declared after m_windowFactory, so
    // reverse-order destruction would destroy the compositions first. Do not
    // make resetRuntime optional or rely on member order for this pair.
    std::unique_ptr<AudioAppletComposition> m_audioApplet;
    std::unique_ptr<BluetoothAppletComposition> m_bluetoothApplet;
    std::unique_ptr<ClipboardAppletComposition> m_clipboardApplet;
    std::unique_ptr<PowerAppletComposition> m_powerApplet;
    std::unique_ptr<LauncherAppletComposition> m_launcherApplet;
    std::unique_ptr<GlobalMenuAppletComposition> m_globalMenuApplet;
    std::unique_ptr<TaskListAppletComposition> m_taskListApplet;
    std::unique_ptr<StatusNotifierAppletComposition> m_statusNotifierApplet;
    std::unique_ptr<NotificationWindowController> m_notificationWindows;
    std::unique_ptr<ShellDevelopmentEvidence> m_shellDevelopmentEvidence;
    std::unique_ptr<KGlobalAccelShortcutRegistrar> m_globalShortcutRegistrar;
    std::unique_ptr<NotificationCenterShortcut> m_notificationCenterShortcut;
    QTimer m_outputDebounce;
    QTimer m_windowActionsRetry;
};

} // namespace QindaQt::Shell
