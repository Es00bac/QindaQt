// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"
#include "wallpapercontroller.h"

#include "../common/catalogpaths.h"
#include "../common/shelltokenpublisher.h"
#include "audioappletcomposition.h"
#include "bluetoothappletcomposition.h"
#include "desktopcontrolscomposition.h"
#include "power_applet_controller.h"
#include "qindaqt/shell/desktop_controls/desktop_controls_access.h"
#include "qindaqt/shell/desktop_surface/desktop_surface_controller.h"
#include "globalmenuappletcomposition.h"
#include "kglobalaccelshortcutregistrar.h"
#include "launcherappletcomposition.h"
#include "launcher_persistence.h"
#include "notificationcenterappletaccess.h"
#include "notificationcentershortcut.h"
#include "runtime_layout_adoption.h"
#include "notificationwindowcontroller.h"
#include "notificationquietingsettingsbridge.h"
#include "panelvisibilityruntime.h"
#include "powerappletcomposition.h"
#include "qtcompositoroutputauthority.h"
#include "runtimepanelwindowfactory.h"
#include "shellappearancebridge.h"
#include "shelldevelopmentevidence.h"
#include "shellstartuppreferences.h"
#include "settingsroutelauncher.h"
#include "tasklistappletcomposition.h"
#include "taskorderpersistence.h"
#include "panelquickconfig.h"

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
#include <QDBusConnection>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QScreen>
#include <QStandardPaths>
#include <QTextStream>

#include <utility>

namespace QindaQt::Shell {

// Bounded wait for the one confirmed Settings1 snapshot that precedes catalog
// selection. Startup proceeds on built-in defaults when it expires.
constexpr int StartupPreferenceTimeoutMilliseconds = 2'000;

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
    m_profileAdoptDebounce.setSingleShot(true);
    m_profileAdoptDebounce.setInterval(250);
    connect(&m_profileAdoptDebounce, &QTimer::timeout, this,
            [this] { adoptLayoutProfile(QString()); });
    // The Customize route writes the user-store copy before committing the
    // Settings1 selection, so content edits surface as store changes while
    // selection edits surface through the preference bridge.
    connect(&m_profileStoreWatch, &QFileSystemWatcher::directoryChanged,
            &m_profileAdoptDebounce, [this](const QString &) {
                m_profileAdoptDebounce.start();
            });
    connect(&m_profileStoreWatch, &QFileSystemWatcher::fileChanged,
            &m_profileAdoptDebounce, [this](const QString &) {
                m_profileAdoptDebounce.start();
            });
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
    // AGENT-CONTRACT: The confirmed Settings1 layout/appearance selection is
    // read before the initial surface plan so an ordinary restart adopts what
    // the Customize route saved (project audit A05/A07). Explicit CLI options
    // outrank these values. --list stays a deterministic inspection tool and
    // never touches the service.
    if (!parsed.options->listOnly) {
        QString preferenceError;
        m_startupPreferences = readConfirmedShellPreferences(
            QDBusConnection::sessionBus(), StartupPreferenceTimeoutMilliseconds,
            &preferenceError);
        if (!m_startupPreferences.has_value()) {
            qWarning().noquote()
                << "QindaQt shell starts with built-in profile and theme"
                   " defaults; no confirmed Settings1 preferences:"
                << preferenceError;
        }
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
    // AGENT-CONTRACT: Profile catalogs merge low-to-high precedence with the
    // user store last, matching the Settings Customize route contract (project
    // audit A06). The source tree participates only for the genuine
    // build-tree executable so an installed shell never shadows user profiles.
    const QString sourceProfileDirectory = buildTreeSourceDirectory(
        QINDAQT_SOURCE_PROFILE_DIR, QINDAQT_SHELL_BUILD_EXECUTABLE_PATH);
    const QStringList profileDirectories = resolveProfileCatalogDirectories(
        options.profileDirectory, sourceProfileDirectory);
    m_profileCatalogDirectories = profileDirectories;
    m_profileLockedByCli = !options.profileId.isEmpty();
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
    if (!m_profiles.loadDirectories(profileDirectories, error) ||
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
    const QString requestedProfile =
        resolveStartupProfileId(options.profileId, m_startupPreferences);
    if (!m_profiles.selectById(requestedProfile)) {
        // AGENT-GUARD: Only an explicit --profile may fail startup. A saved
        // selection whose profile was deleted or renamed must recover to the
        // built-in default instead of stranding the session without a shell.
        if (!options.profileId.isEmpty() || !m_startupPreferences.has_value()) {
            *error = QStringLiteral("Unknown profile: %1").arg(requestedProfile);
            return false;
        }
        qWarning().noquote()
            << "QindaQt shell could not honor the saved profile selection;"
               " falling back to the default profile:"
            << requestedProfile;
        if (!m_profiles.selectById(QStringLiteral("qindaqt"))) {
            *error = QStringLiteral("Unknown profile: qindaqt");
            return false;
        }
    }

    const QString profileDefaultTheme =
        m_profiles.current().value(QStringLiteral("defaultTheme")).toString();
    QString requestedTheme = resolveStartupThemeId(
        options.themeId, m_startupPreferences, profileDefaultTheme);
    if (!m_themes.selectById(requestedTheme)) {
        // AGENT-GUARD: Only an explicit --theme may fail startup. A saved
        // preference naming an uninstalled theme fails closed to the selected
        // profile's default rather than leaving the session without a shell.
        if (!options.themeId.isEmpty() || !m_startupPreferences.has_value()
            || requestedTheme == profileDefaultTheme) {
            *error = QStringLiteral("Unknown theme: %1").arg(requestedTheme);
            return false;
        }
        qWarning().noquote()
            << "QindaQt shell could not honor the saved theme preference;"
               " falling back to the profile default:"
            << requestedTheme;
        requestedTheme = profileDefaultTheme;
        if (!m_themes.selectById(requestedTheme)) {
            *error = QStringLiteral("Unknown theme: %1").arg(requestedTheme);
            return false;
        }
    }
    m_dataRoots = ShellIconConfiguration::dataRoots(
        QProcessEnvironment::systemEnvironment(), QDir::homePath());
    refreshProfileStoreWatch();
    return true;
}

void ShellRuntimeApplication::adoptLayoutProfile(
    const QString &preferredProfileId)
{
    QString diagnostic;
    const auto outcome = RuntimeLayoutAdoption::reloadAndSelect(
        m_profiles, m_profileCatalogDirectories, preferredProfileId,
        m_profileLockedByCli, &diagnostic);
    refreshProfileStoreWatch();
    if (outcome == RuntimeLayoutAdoption::Outcome::Failed) {
        qWarning().noquote()
            << "QindaQt shell kept its prior layout:" << diagnostic;
        return;
    }
    if (outcome == RuntimeLayoutAdoption::Outcome::LockedByCommandLine) {
        qWarning().noquote()
            << "QindaQt shell ignored a saved layout change:" << diagnostic;
        return;
    }
    if (outcome == RuntimeLayoutAdoption::Outcome::KeptPriorSelection) {
        qWarning().noquote()
            << "QindaQt shell kept its prior layout:" << diagnostic;
    }
    const int index = m_profiles.currentIndex();
    if (index < 0
        || static_cast<qsizetype>(index) >= m_profiles.profiles().size()) {
        return;
    }
    const auto &profile = m_profiles.profiles().at(index);
    if (m_panelVisibility) {
        m_panelVisibility->applyProfile(profile);
    }
    // The window factory's resolved inventory must follow the adopted
    // profile before reconciliation: new panel ids cannot create windows
    // from the startup inventory, and kept panels pick up edited applet
    // sets through their live panel maps.
    if (m_windowFactory) {
        m_windowFactory->adoptProfile(profile, m_applets, m_appletPolicy);
    }
    if (m_desktopSurface) {
        m_desktopSurface->adoptProfile(profile, m_applets, m_appletPolicy);
    }
    followGlobalMenuLayout(profile);
    qInfo().noquote() << "QindaQt shell adopted layout profile" << profile.id;
    scheduleOutputReconcile();
}

void ShellRuntimeApplication::refreshProfileStoreWatch()
{
    const QString userProfiles =
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
            .filePath(QStringLiteral("qindaqt/profiles"));
    QStringList paths;
    if (QFileInfo::exists(userProfiles)) {
        // Watch the directory and each stored profile: a rewrite in place
        // only reports through fileChanged, an add or rename through
        // directoryChanged.
        paths.append(userProfiles);
        const auto entries = QDir(userProfiles).entryInfoList(
            {QStringLiteral("*.json")}, QDir::Files);
        for (const auto &entry : entries) {
            paths.append(entry.absoluteFilePath());
        }
    } else {
        // The store directory appears with the first Apply; until then watch
        // its parent so adoption starts with the first saved layout.
        const QString parent = QFileInfo(userProfiles).absolutePath();
        if (QFileInfo::exists(parent)) {
            paths.append(parent);
        }
    }
    if (m_profileStoreWatch.directories() != paths) {
        const auto previous = m_profileStoreWatch.files()
            + m_profileStoreWatch.directories();
        if (!previous.isEmpty()) {
            m_profileStoreWatch.removePaths(previous);
        }
        if (!paths.isEmpty()) {
            m_profileStoreWatch.addPaths(paths);
        }
    }
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

QVariantMap ShellRuntimeApplication::effectiveThemeMap() const
{
    QVariantMap theme = m_themes.current();
    // The confirmed fonts.family preference overlays the theme's own family,
    // mirroring the token publication overlay so raw maps and QST agree.
    const std::optional<ShellPreferenceValues> &confirmed =
        m_appearanceBridge && m_appearanceBridge->lastConfirmed().has_value()
            ? m_appearanceBridge->lastConfirmed()
            : m_startupPreferences;
    if (confirmed.has_value() && !confirmed->fontFamily.isEmpty()) {
        theme.insert(QStringLiteral("fontFamily"), confirmed->fontFamily);
    }
    return theme;
}

void ShellRuntimeApplication::propagateThemeToSurfaces()
{
    const QVariantMap theme = effectiveThemeMap();
    if (m_windowFactory) {
        m_windowFactory->setTheme(theme);
    }
    if (m_notificationWindows) {
        m_notificationWindows->setTheme(theme);
    }
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
    // The compositor may not have observed the new dock yet. Retry the same
    // client after its deadline; never add a binding or retain failed truth.
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

void ShellRuntimeApplication::initializeDesktopControls(
    std::optional<qint64> compositorProcessId)
{
    // AGENT-GUARD: desktop controls borrow these applet facades. Construct
    // after them and destroy after panel windows, before any borrowed owner.
    m_desktopControls = std::make_unique<DesktopControlsComposition>(
        m_applets, m_appletPolicy, QDBusConnection::sessionBus(),
        compositorProcessId,
        DesktopControlsComposition::BorrowedFacades{
            m_launcherApplet->access(), m_globalMenuApplet->access(),
            m_taskListApplet->access(), m_audioApplet->access(),
            m_bluetoothApplet->access(), m_powerApplet->access(),
            m_powerApplet->access() ? m_powerApplet->access()->sessionActions() : nullptr});
    QString desktopControlsError;
    if (!m_desktopControls->start(&desktopControlsError)) {
        qWarning().noquote() << "QindaQt shell could not start desktop controls:"
                             << desktopControlsError;
    }
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
    if (!initializeTokens(error)) {
        resetRuntime();
        return false;
    }
    if (!initializeIcons(error)) { resetRuntime(); return false; }
    if (!initializeLauncherRuntime(error)) {
        resetRuntime();
        return false;
    }
    initializeServiceAppletCompositions(profile);
    initializeDesktopControls(options.compositorProcessId);
    if (m_presentationAccessToken) {
        // Options treat these values as one trust bundle. Fail closed here so
        // alternate callers cannot present without compositor identity.
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
                *m_quietingSettingsClient, *m_notificationInterruptionPolicy);
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
        // The edge signal omits initial Unknown. Mirror both fail-closed
        // defaults before D-Bus work can complete.
        m_notificationPrivacyPolicy->setPrivatePresentationAllowed(
            m_sessionLockMonitor->contentMayBeShown());
        m_notificationCenterAccess->publishPrivatePresentationAllowed(
            m_notificationPrivacyPolicy->privatePresentationAllowed());
        m_notificationCenterAccess->publishDoNotDisturbEnabled(
            m_notificationInterruptionPolicy->doNotDisturbEnabled());
    }
    m_windowFactory =
        std::make_unique<RuntimePanelWindowFactory>(
            m_engine, profile, effectiveThemeMap(), m_applets, m_appletPolicy,
            m_notificationCenterAccess.get(), m_audioApplet->access(),
            m_bluetoothApplet->access(), m_powerApplet->access(),
            m_launcherApplet->access(), m_globalMenuApplet->access(), m_clipboardApplet->access(),
            m_taskListApplet->access(),
            m_statusNotifierApplet->access());
    m_windowFactory->setDesktopControlsAccess(m_desktopControls->access());
    // The panel right-click configuration facade composes over the shared
    // Settings1 client and the Settings route launcher (both owned above).
    m_panelQuickConfig = std::make_unique<PanelQuickConfig>(*m_settingsClient,
                                                            m_settingsRouteLauncher.get());
    m_panelQuickConfig->start();
    m_windowFactory->setPanelQuickConfig(m_panelQuickConfig.get());
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

    startSettingsClients();
    initializeWallpaper();

    initializeDesktopSurface(profile);

    initializeAppearanceBridge(!options.themeId.isEmpty());

    if (m_notificationClient) {
        startNotificationOutputAuthority();
        QString lockError;
        if (!m_sessionLockMonitor->start(&lockError)) {
            // Lock observation is a privacy gate, not essential panel work.
            // Bus failure keeps policy denied while the shell stays available.
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
            effectiveThemeMap());
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
    m_profileAdoptDebounce.stop();
    m_wallpaper.reset();
    // Borrowed facades (desktop controls, launcher) are released further
    // down; the desktop surface must not outlive them (AGENT-GUARD on the
    // member declaration).
    m_desktopSurface.reset();
    m_windowActionsRetry.stop();
    m_notificationCenterShortcut.reset();
    m_globalShortcutRegistrar.reset();
    m_shellDevelopmentEvidence.reset();
    m_notificationWindows.reset();
    m_panelQuickConfig.reset();
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
    m_desktopControls.reset();
    m_statusNotifierApplet.reset();
    m_taskListApplet.reset();
    m_globalMenuApplet.reset();
    if (m_windowActionsClient) {
        m_windowActionsClient->stop();
    }
    m_windowActionsClient.reset();
    m_windowActionsTransport.reset();
    m_launcherApplet.reset();
    m_clipboardApplet.reset();
    m_audioApplet.reset();
    m_bluetoothApplet.reset();
    m_powerApplet.reset();
    m_notificationCenterAccess.reset();
    // The bridge borrows the token publisher and theme catalog; release it
    // before either can disappear.
    m_appearanceBridge.reset();
    if (m_settingsClient) {
        m_settingsClient->stop();
    }
    if (m_quietingSettingsClient) {
        m_quietingSettingsClient->stop();
    }
    m_quietingSettingsClient.reset();
    m_quietingSettingsTransport.reset();
    m_settingsClient.reset();
    m_settingsTransport.reset();
    m_notificationPresentation.reset();
    m_notificationPrivacyPolicy.reset();
    m_notificationInterruptionPolicy.reset();
    m_sessionLockMonitor.reset();
    m_sessionLockTransport.reset();
    m_notificationClient.reset();
    m_notificationTransport.reset();
    m_tokenPublisher.reset();
}

bool ShellRuntimeApplication::settlePanelVisibility(QString *error)
{
    if (!m_panelVisibility || !m_controller) {
        return true;
    }
    const bool authorityAvailable = m_visibilityClient
        && !m_visibilityClient->safeVisibleRequired()
        && m_visibilityClient->snapshot().has_value();
    // A hide may acquire an animation hold after planning. Reevaluate in the
    // same turn so no intermediate unmapped frame is painted.
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
