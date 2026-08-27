// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "runtimeoptions.h"

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

namespace QindaQt::Services::SessionLockState {
class QtSessionLockTransport;
class SessionLockStateMonitor;
}

namespace QindaQt::Shell {

class RuntimePanelWindowFactory;
class KGlobalAccelShortcutRegistrar;
class NotificationCenterAppletAccess;
class NotificationCenterShortcut;
class NotificationWindowController;

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
    [[nodiscard]] bool reconcileSurfaces(QString *error);
    void attachOutputSignals(QScreen *screen);
    void scheduleOutputReconcile();
    void resetRuntime();

    QGuiApplication &m_application;
    Profiles::ProfileCatalog m_profiles;
    Themes::ThemeCatalog m_themes;
    Applets::ManifestCatalog m_applets;
    AppletHost::CapabilityPolicy m_appletPolicy;
    QQmlEngine m_engine;
    std::unique_ptr<RuntimePanelWindowFactory> m_windowFactory;
    std::unique_ptr<ShellSurface::LayerShellSurfaceBackend> m_backend;
    std::unique_ptr<ShellSurface::PanelSurfaceController> m_controller;
    std::unique_ptr<ShellVisibilityClient::QtCompositorVisibilityTransport>
        m_visibilityTransport;
    std::unique_ptr<ShellVisibilityClient::CompositorVisibilityClient>
        m_visibilityClient;
    std::unique_ptr<ShellOrchestration::PanelInteractionStore> m_interactions;
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
    std::unique_ptr<Services::NotificationPresentationModel::
                        NotificationPresentationController>
        m_notificationPresentation;
    std::unique_ptr<NotificationCenterAppletAccess> m_notificationCenterAccess;
    std::unique_ptr<NotificationWindowController> m_notificationWindows;
    std::unique_ptr<KGlobalAccelShortcutRegistrar> m_globalShortcutRegistrar;
    std::unique_ptr<NotificationCenterShortcut> m_notificationCenterShortcut;
    QTimer m_outputDebounce;
};

} // namespace QindaQt::Shell
