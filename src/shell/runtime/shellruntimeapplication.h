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

namespace QindaQt::Shell {

class RuntimePanelWindowFactory;
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
    [[nodiscard]] bool initializeRuntime(QString *error);
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
    std::unique_ptr<Services::NotificationPresentationModel::
                        NotificationPresentationController>
        m_notificationPresentation;
    std::unique_ptr<NotificationWindowController> m_notificationWindows;
    QTimer m_outputDebounce;
};

} // namespace QindaQt::Shell
