// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "../common/catalogpaths.h"
#include "audioappletcomposition.h"
#include "bluetoothappletcomposition.h"
#include "globalmenuappletcomposition.h"
#include "kglobalaccelshortcutregistrar.h"
#include "launcherappletcomposition.h"
#include "launcher_persistence.h"
#include "notificationcenterappletaccess.h"
#include "notificationcentershortcut.h"
#include "notificationwindowcontroller.h"
#include "notificationquietingsettingsbridge.h"
#include "panelvisibilityruntime.h"
#include "powerappletcomposition.h"
#include "qtcompositoroutputauthority.h"
#include "runtimepanelwindowfactory.h"
#include "shelldevelopmentevidence.h"
#include "settingsroutelauncher.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/services/notification_presentation/presentation_token_channel.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/qt_notification_presentation_transport.h"
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"
#include "qindaqt/services/session_lock_state/qt_session_lock_transport.h"
#include "qindaqt/services/session_lock_state/session_lock_state_monitor.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell_orchestration/panel_interaction_store.h"
#include "qindaqt/shell_surface/layer_shell_surface_backend.h"
#include "qindaqt/shell_surface/panel_surface_controller.h"
#include "qindaqt/shell_visibility_client/compositor_visibility_client.h"
#include "qindaqt/shell_visibility_client/qt_compositor_visibility_transport.h"
#include "qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QScreen>
#include <QTextStream>

#include <utility>

namespace QindaQt::Shell {

ShellRuntimeApplication::ShellRuntimeApplication(QGuiApplication &application)
    : m_application(application)
{
    m_outputDebounce.setSingleShot(true);
    m_outputDebounce.setInterval(0);
    connect(&m_outputDebounce, &QTimer::timeout, this, [this] {
        QString error;
        if (!reconcileSurfaces(&error) || !settlePanelVisibility(&error)) {
            qWarning().noquote() << "QindaQt shell kept its prior surface set:" << error;
        }
    });
    m_windowActionsRetry.setSingleShot(true);
    m_windowActionsRetry.setInterval(2'500);
    connect(&m_windowActionsRetry, &QTimer::timeout, this,
            &ShellRuntimeApplication::restartWindowActionsIdentity);
}

ShellRuntimeApplication::~ShellRuntimeApplication()
{
    resetRuntime();
}

int ShellRuntimeApplication::run()
{
    const RuntimeOptionsResult parsed = parseRuntimeOptions(m_application);
    if (!parsed.options.has_value()) {
        qCritical().noquote() << parsed.error;
        return 2;
    }

    QString error;
    if (!loadPresentationToken(*parsed.options, &error)) {
        qCritical().noquote() << error;
        return 2;
    }
    if (!loadCatalogs(*parsed.options, &error)) {
        qCritical().noquote() << error;
        return 2;
    }
    if (parsed.options->listOnly) {
        printCatalog();
        return 0;
    }
    if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        qCritical().noquote()
            << "qindaqt-shell requires Qt's Wayland platform; use qindaqt-shell-preview for"
               " offscreen or X11 development";
        return 3;
    }
    if (!initializeRuntime(*parsed.options, &error)) {
        qCritical().noquote() << error;
        return 4;
    }
    return m_application.exec();
}

bool ShellRuntimeApplication::loadPresentationToken(
    const RuntimeOptions &options, QString *error)
{
    m_presentationAccessToken.reset();
    if (options.presentationTokenDescriptor < 0) {
        return true;
    }
    auto result = Services::NotificationPresentation::PresentationTokenChannel::
        readAndClose(options.presentationTokenDescriptor);
    if (!result.ok()) {
        *error = std::move(result.message);
        return false;
    }
    m_presentationAccessToken = std::move(result.token);
    return true;
}

bool ShellRuntimeApplication::loadCatalogs(const RuntimeOptions &options, QString *error)
{
    const QString profileDirectory = resolveCatalogDataDirectory(options.profileDirectory,
                                                                 "QINDAQT_PROFILE_DIR",
                                                                 QINDAQT_SOURCE_PROFILE_DIR,
                                                                 QStringLiteral("qindaqt/profiles"));
    const QString themeDirectory = resolveCatalogDataDirectory(options.themeDirectory,
                                                               "QINDAQT_THEME_DIR",
                                                               QINDAQT_SOURCE_THEME_DIR,
                                                               QStringLiteral("qindaqt/themes"));
    const QString appletDirectory = resolveCatalogDataDirectory(
        options.appletDirectory, "QINDAQT_APPLET_DIR", QINDAQT_SOURCE_APPLET_DIR,
        QStringLiteral("qindaqt/applets"));
    const QString appletPolicyFile = resolveCatalogDataFile(
        options.appletPolicyFile, "QINDAQT_APPLET_POLICY",
        QINDAQT_SOURCE_APPLET_POLICY,
        QStringLiteral("qindaqt/applet-policy/default.json"));
    if (!m_profiles.loadDirectory(profileDirectory, error) ||
        !m_themes.loadDirectory(themeDirectory, error) ||
        !m_applets.loadDirectory(appletDirectory, error)) {
        return false;
    }
    const auto loadedPolicy =
        AppletHost::CapabilityPolicyLoader::fromFile(appletPolicyFile);
    if (!loadedPolicy.ok) {
        *error = loadedPolicy.error;
        return false;
    }
    m_appletPolicy = loadedPolicy.policy;
    if (!m_profiles.selectById(options.profileId)) {
        *error = QStringLiteral("Unknown profile: %1").arg(options.profileId);
        return false;
    }

    const QString requestedTheme = !options.themeId.isEmpty()
        ? options.themeId
        : m_profiles.current().value(QStringLiteral("defaultTheme")).toString();
    if (!m_themes.selectById(requestedTheme)) {
        *error = QStringLiteral("Unknown theme: %1").arg(requestedTheme);
        return false;
    }
    return true;
}

void ShellRuntimeApplication::printCatalog() const
{
    QTextStream output(stdout);
    output << "Profiles:\n";
    for (const auto &profile : m_profiles.profiles()) {
        output << "  " << profile.id << " - " << profile.name << '\n';
    }
    output << "Themes:\n";
    for (const auto &theme : m_themes.themes()) {
        output << "  " << theme.id << " - " << theme.name << '\n';
    }
    output << "Applets:\n";
    for (const auto &applet : m_applets.manifests()) {
        output << "  " << applet.id << " - " << applet.name << '\n';
    }
}

bool ShellRuntimeApplication::initializeLauncherRuntime(QString *error)
{
    m_settingsTransport = std::make_unique<Services::SettingsClient::QtSettingsTransport>(
        QDBusConnection::sessionBus());
    m_settingsClient = std::make_unique<Services::SettingsClient::SettingsClient>(
        *m_settingsTransport,
        QStringList{QStringLiteral("services.doNotDisturb"),
                    QStringLiteral("accessibility.reducedMotion"),
                    QStringLiteral("panels.autoHideDelayMs"),
                    Launcher::LauncherPersistenceController::pinnedKey(),
                    Launcher::LauncherPersistenceController::recentKey()});
    m_launcherApplet = std::make_unique<LauncherAppletComposition>(
        m_applets, m_appletPolicy,
        launcherDataRoots(QProcessEnvironment::systemEnvironment(),
                          QDir::homePath()),
        *m_settingsClient, QDBusConnection::sessionBus());
    return m_launcherApplet->start(error);
}

void ShellRuntimeApplication::initializeServiceAppletCompositions()
{
    m_audioApplet =
        std::make_unique<AudioAppletComposition>(m_applets, m_appletPolicy);
    m_bluetoothApplet =
        std::make_unique<BluetoothAppletComposition>(m_applets, m_appletPolicy);
    m_powerApplet =
        std::make_unique<PowerAppletComposition>(m_applets, m_appletPolicy);
    const QDBusConnection sessionBus = QDBusConnection::sessionBus();
    m_windowActionsTransport = std::make_unique<
        ShellWindowActionsClient::QtShellWindowActionsTransport>(sessionBus);
    m_windowActionsClient = std::make_unique<
        ShellWindowActionsClient::ShellWindowActionsClient>(
            *m_windowActionsTransport);
    m_globalMenuApplet = std::make_unique<GlobalMenuAppletComposition>(
        m_applets, m_appletPolicy, sessionBus, *m_windowActionsClient);
    m_globalMenuApplet->start();
    connect(m_windowActionsClient.get(),
            &ShellWindowActionsClient::ShellWindowActionsClient::identityChanged,
            this, [this] {
                if (m_windowActionsClient->identitySnapshot()) {
                    m_windowActionsRetry.stop();
                } else {
                    m_windowActionsRetry.start();
                }
            });
}

void ShellRuntimeApplication::restartWindowActionsIdentity()
{
    if (!m_windowActionsClient) {
        return;
    }
    m_windowActionsClient->stop();
    QString error;
    if (!m_windowActionsClient->start(&error)) {
        qWarning().noquote()
            << "QindaQt shell could not start authenticated window identity:"
            << error;
        m_windowActionsRetry.start();
        return;
    }
    // The compositor may not yet have observed the just-mapped dock surface.
    // Retry the same client after its request deadline; never create a second
    // binding or retain a failed snapshot.
    if (!m_windowActionsClient->identitySnapshot()) {
        m_windowActionsRetry.start();
    }
}

void ShellRuntimeApplication::initializePanelVisibility(
    const Profiles::LayoutProfile &profile)
{
    m_interactions = std::make_unique<ShellOrchestration::PanelInteractionStore>();
    m_panelVisibilityShortcutRegistrar =
        std::make_unique<KGlobalAccelShortcutRegistrar>();
    m_panelVisibility = std::make_unique<PanelVisibilityRuntime>(
        m_application, *m_interactions, *m_settingsClient,
        *m_panelVisibilityShortcutRegistrar, profile,
        m_themes.current().value(QStringLiteral("motionDuration")).toInt());
    connect(m_interactions.get(),
            &ShellOrchestration::PanelInteractionStore::interactionsChanged,
            this, &ShellRuntimeApplication::scheduleOutputReconcile);
}

bool ShellRuntimeApplication::initializeRuntime(const RuntimeOptions &options,
                                                QString *error)
{
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex < 0 ||
        static_cast<qsizetype>(profileIndex) >= m_profiles.profiles().size()) {
        *error = QStringLiteral("selected profile catalog state is invalid");
        return false;
    }

    const auto &profile = m_profiles.profiles().at(profileIndex);
    if (!initializeLauncherRuntime(error)) {
        resetRuntime();
        return false;
    }
    initializeServiceAppletCompositions();
    if (m_presentationAccessToken) {
        // Runtime option parsing treats these values as one trust bundle. Keep
        // the assertion fail closed here too so future alternate callers cannot
        // accidentally construct a presenter without a compositor identity.
        if (!options.compositorProcessId.has_value()) {
            *error = QStringLiteral(
                "notification presentation requires a compositor process id");
            return false;
        }
        m_notificationTransport = std::make_unique<Services::
            NotificationPresentationClient::QtNotificationPresentationTransport>();
        m_notificationClient = std::make_unique<Services::
            NotificationPresentationClient::NotificationPresentationClient>(
                *m_notificationTransport, std::move(*m_presentationAccessToken));
        m_presentationAccessToken.reset();
        m_sessionLockTransport = std::make_unique<Services::SessionLockState::
            QtSessionLockTransport>();
        m_sessionLockMonitor = std::make_unique<Services::SessionLockState::
            SessionLockStateMonitor>(*m_sessionLockTransport,
                                     *options.compositorProcessId);
        m_notificationInterruptionPolicy = std::make_unique<Services::
            NotificationPresentationPolicy::NotificationInterruptionPolicy>();
        m_notificationPrivacyPolicy = std::make_unique<Services::
            NotificationPresentationPolicy::NotificationPrivacyPolicy>();
        m_quietingSettingsBridge =
            std::make_unique<NotificationQuietingSettingsBridge>(
                *m_settingsClient, *m_notificationInterruptionPolicy);
        m_settingsRouteLauncher = std::make_unique<SettingsRouteLauncher>();
        m_notificationPresentation = std::make_unique<Services::
            NotificationPresentationModel::NotificationPresentationController>(
                *m_notificationClient, *m_notificationInterruptionPolicy,
                *m_notificationPrivacyPolicy);
        m_notificationCenterAccess =
            std::make_unique<NotificationCenterAppletAccess>();
        connect(m_notificationCenterAccess.get(),
                &NotificationCenterAppletAccess::toggleRequested,
                m_notificationPresentation.get(),
                &Services::NotificationPresentationModel::
                    NotificationPresentationController::toggleCenter);
        connect(m_notificationPresentation.get(),
                &Services::NotificationPresentationModel::
                    NotificationPresentationController::centerOpenChanged,
                m_notificationCenterAccess.get(), [this] {
                    m_notificationCenterAccess->publishCenterOpen(
                        m_notificationPresentation->centerOpen());
                });
        connect(m_notificationInterruptionPolicy.get(),
                &Services::NotificationPresentationPolicy::
                    NotificationInterruptionPolicy::doNotDisturbEnabledChanged,
                m_notificationCenterAccess.get(), [this](bool enabled) {
                    m_notificationCenterAccess->publishDoNotDisturbEnabled(enabled);
                });
        connect(m_sessionLockMonitor.get(),
                &Services::SessionLockState::SessionLockStateMonitor::
                    contentMayBeShownChanged,
                m_notificationPrivacyPolicy.get(),
                &Services::NotificationPresentationPolicy::
                    NotificationPrivacyPolicy::setPrivatePresentationAllowed);
        connect(m_notificationPrivacyPolicy.get(),
                &Services::NotificationPresentationPolicy::
                    NotificationPrivacyPolicy::privatePresentationAllowedChanged,
                m_notificationCenterAccess.get(),
                &NotificationCenterAppletAccess::
                    publishPrivatePresentationAllowed);
        // The monitor's edge-triggered signal does not publish its initial
        // Unknown state. Mirror both fail-closed defaults explicitly before
        // any D-Bus work can complete.
        m_notificationPrivacyPolicy->setPrivatePresentationAllowed(
            m_sessionLockMonitor->contentMayBeShown());
        m_notificationCenterAccess->publishPrivatePresentationAllowed(
            m_notificationPrivacyPolicy->privatePresentationAllowed());
        m_notificationCenterAccess->publishDoNotDisturbEnabled(
            m_notificationInterruptionPolicy->doNotDisturbEnabled());
    }
    m_windowFactory =
        std::make_unique<RuntimePanelWindowFactory>(
            m_engine, profile, m_themes.current(), m_applets, m_appletPolicy,
            m_notificationCenterAccess.get(), m_audioApplet->access(),
            m_bluetoothApplet->access(), m_powerApplet->access(),
            m_launcherApplet->access(), m_globalMenuApplet->access());
    m_backend =
        std::make_unique<ShellSurface::LayerShellSurfaceBackend>(*m_windowFactory);
    m_controller = std::make_unique<ShellSurface::PanelSurfaceController>(*m_backend);
    m_visibilityTransport = std::make_unique<
        ShellVisibilityClient::QtCompositorVisibilityTransport>();
    m_visibilityClient = std::make_unique<
        ShellVisibilityClient::CompositorVisibilityClient>(*m_visibilityTransport);
    initializePanelVisibility(profile);
    connect(m_visibilityClient.get(),
            &ShellVisibilityClient::CompositorVisibilityClient::stateChanged,
            this, &ShellRuntimeApplication::scheduleOutputReconcile);

    if (!m_visibilityClient->start(error)) {
        resetRuntime();
        return false;
    }

    QString settingsError;
    if (!m_settingsClient->start(&settingsError)) {
        qWarning().noquote()
            << "QindaQt shell could not start Settings1; launcher persistence"
               " and notification quieting remain unavailable:"
            << settingsError;
    }

    if (m_notificationClient) {
        startNotificationOutputAuthority();
        QString lockError;
        if (!m_sessionLockMonitor->start(&lockError)) {
            // Lock observation is a privacy gate, not an essential panel
            // process. A local/session-bus failure keeps the policy denied
            // while the remainder of the shell stays available.
            qWarning().noquote()
                << "QindaQt shell could not start authenticated lock-state"
                   " observation; notification presentation remains private:"
                << lockError;
        }
        if (!m_notificationClient->start(error)) {
            resetRuntime();
            return false;
        }
        m_notificationWindows = std::make_unique<NotificationWindowController>(
            m_engine, *m_notificationPresentation,
            m_quietingSettingsBridge->controller(), *m_settingsRouteLauncher,
            m_themes.current());
        m_globalShortcutRegistrar =
            std::make_unique<KGlobalAccelShortcutRegistrar>();
        m_notificationCenterShortcut =
            std::make_unique<NotificationCenterShortcut>(
                *m_globalShortcutRegistrar,
                [this] { m_notificationCenterAccess->toggle(); });
        if (!m_notificationCenterShortcut->registrationRequestAccepted()) {
            qWarning().noquote()
                << "QindaQt shell could not submit the notification-center"
                   " global shortcut; the panel entry remains available";
        }
    }

    if (!reconcileSurfaces(error) || !settlePanelVisibility(error)) {
        resetRuntime();
        return false;
    }
    restartWindowActionsIdentity();

    for (QScreen *screen : m_application.screens()) {
        attachOutputSignals(screen);
    }
    connect(&m_application, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        attachOutputSignals(screen);
        scheduleOutputReconcile();
    });
    connect(&m_application, &QGuiApplication::screenRemoved, this,
            [this](QScreen *) { scheduleOutputReconcile(); });
    connect(&m_application, &QGuiApplication::primaryScreenChanged, this,
            [this](QScreen *) { scheduleOutputReconcile(); });
    if (!startDevelopmentEvidence(options, error)) {
        resetRuntime();
        return false;
    }
    return true;
}

void ShellRuntimeApplication::resetRuntime()
{
    m_outputDebounce.stop();
    m_windowActionsRetry.stop();
    m_notificationCenterShortcut.reset();
    m_globalShortcutRegistrar.reset();
    m_shellDevelopmentEvidence.reset();
    m_notificationWindows.reset();
    m_settingsRouteLauncher.reset();
    m_quietingSettingsBridge.reset();
    m_outputAuthority.reset();
    m_panelVisibility.reset();
    m_panelVisibilityShortcutRegistrar.reset();
    m_interactions.reset();
    m_visibilityClient.reset();
    m_visibilityTransport.reset();
    m_controller.reset();
    m_backend.reset();
    m_windowFactory.reset();
    m_globalMenuApplet.reset();
    if (m_windowActionsClient) {
        m_windowActionsClient->stop();
    }
    m_windowActionsClient.reset();
    m_windowActionsTransport.reset();
    m_launcherApplet.reset();
    m_audioApplet.reset();
    m_bluetoothApplet.reset();
    m_powerApplet.reset();
    m_notificationCenterAccess.reset();
    if (m_settingsClient) {
        m_settingsClient->stop();
    }
    m_settingsClient.reset();
    m_settingsTransport.reset();
    m_notificationPresentation.reset();
    m_notificationPrivacyPolicy.reset();
    m_notificationInterruptionPolicy.reset();
    m_sessionLockMonitor.reset();
    m_sessionLockTransport.reset();
    m_notificationClient.reset();
    m_notificationTransport.reset();
}

bool ShellRuntimeApplication::settlePanelVisibility(QString *error)
{
    if (!m_panelVisibility || !m_controller) {
        return true;
    }
    const bool authorityAvailable = m_visibilityClient
        && !m_visibilityClient->safeVisibleRequired()
        && m_visibilityClient->snapshot().has_value();
    // One hide transition may acquire an animation hold after the pure plan
    // was applied. Reevaluate in the same event turn so the compositor never
    // paints an intermediate unmapped frame before the animation starts.
    for (int pass = 0; pass != 3; ++pass) {
        bool immediateReconcile = false;
        if (!m_panelVisibility->synchronize(
                m_controller->currentPlan(), authorityAvailable,
                &immediateReconcile, error)) {
            return false;
        }
        if (!immediateReconcile) {
            return true;
        }
        if (!reconcileSurfaces(error)) {
            return false;
        }
    }
    if (error != nullptr) {
        *error = QStringLiteral("panel visibility animation did not settle");
    }
    return false;
}

} // namespace QindaQt::Shell
