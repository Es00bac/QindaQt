// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "../common/catalogpaths.h"
#include "kglobalaccelshortcutregistrar.h"
#include "notificationcenterappletaccess.h"
#include "notificationcentershortcut.h"
#include "notificationwindowcontroller.h"
#include "runtimepanelwindowfactory.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/services/notification_presentation/presentation_token_channel.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/qt_notification_presentation_transport.h"
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"
#include "qindaqt/shell_orchestration/output_inventory_matcher.h"
#include "qindaqt/shell_orchestration/panel_interaction_store.h"
#include "qindaqt/shell_orchestration/panel_runtime_plan_assembler.h"
#include "qindaqt/shell_orchestration/panel_visibility_inventory_assembler.h"
#include "qindaqt/shell_surface/layer_shell_surface_backend.h"
#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"
#include "qindaqt/shell_surface/panel_surface_controller.h"
#include "qindaqt/shell_surface/qt_output_inventory.h"
#include "qindaqt/shell_visibility_client/compositor_visibility_client.h"
#include "qindaqt/shell_visibility_client/qt_compositor_visibility_transport.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QTextStream>

#include <utility>

namespace {

QVector<QindaQt::ShellLayout::LogicalOutput> logicalOutputs(
    const QindaQt::ShellVisibility::CompositorVisibilitySnapshot &snapshot)
{
    QVector<QindaQt::ShellLayout::LogicalOutput> result;
    result.reserve(snapshot.outputs.size());
    for (const auto &output : snapshot.outputs) {
        result.append({output.id, output.geometry, output.scale});
    }
    return result;
}

} // namespace

namespace QindaQt::Shell {

ShellRuntimeApplication::ShellRuntimeApplication(QGuiApplication &application)
    : m_application(application)
{
    m_outputDebounce.setSingleShot(true);
    m_outputDebounce.setInterval(0);
    connect(&m_outputDebounce, &QTimer::timeout, this, [this] {
        QString error;
        if (!reconcileSurfaces(&error)) {
            qWarning().noquote() << "QindaQt shell kept its prior surface set:" << error;
        }
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
    if (!initializeRuntime(&error)) {
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

bool ShellRuntimeApplication::initializeRuntime(QString *error)
{
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex < 0 ||
        static_cast<qsizetype>(profileIndex) >= m_profiles.profiles().size()) {
        *error = QStringLiteral("selected profile catalog state is invalid");
        return false;
    }

    const auto &profile = m_profiles.profiles().at(profileIndex);
    if (m_presentationAccessToken) {
        m_notificationTransport = std::make_unique<Services::
            NotificationPresentationClient::QtNotificationPresentationTransport>();
        m_notificationClient = std::make_unique<Services::
            NotificationPresentationClient::NotificationPresentationClient>(
                *m_notificationTransport, std::move(*m_presentationAccessToken));
        m_presentationAccessToken.reset();
        m_notificationPresentation = std::make_unique<Services::
            NotificationPresentationModel::NotificationPresentationController>(
                *m_notificationClient);
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
    }
    m_windowFactory =
        std::make_unique<RuntimePanelWindowFactory>(
            m_engine, profile, m_themes.current(), m_applets, m_appletPolicy,
            m_notificationCenterAccess.get());
    m_backend =
        std::make_unique<ShellSurface::LayerShellSurfaceBackend>(*m_windowFactory);
    m_controller = std::make_unique<ShellSurface::PanelSurfaceController>(*m_backend);
    m_visibilityTransport = std::make_unique<
        ShellVisibilityClient::QtCompositorVisibilityTransport>();
    m_visibilityClient = std::make_unique<
        ShellVisibilityClient::CompositorVisibilityClient>(*m_visibilityTransport);
    m_interactions = std::make_unique<ShellOrchestration::PanelInteractionStore>();
    connect(m_visibilityClient.get(),
            &ShellVisibilityClient::CompositorVisibilityClient::stateChanged,
            this, &ShellRuntimeApplication::scheduleOutputReconcile);
    connect(m_interactions.get(),
            &ShellOrchestration::PanelInteractionStore::interactionsChanged,
            this, &ShellRuntimeApplication::scheduleOutputReconcile);

    if (!m_visibilityClient->start(error)) {
        resetRuntime();
        return false;
    }

    if (m_notificationClient) {
        if (!m_notificationClient->start(error)) {
            resetRuntime();
            return false;
        }
        m_notificationWindows = std::make_unique<NotificationWindowController>(
            m_engine, *m_notificationPresentation, m_themes.current());
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

    if (!reconcileSurfaces(error)) {
        resetRuntime();
        return false;
    }

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
    return true;
}

bool ShellRuntimeApplication::reconcileSurfaces(QString *error)
{
    const auto inventory = ShellSurface::QtOutputInventory::read();
    if (!inventory.ok()) {
        *error = inventory.error;
        return false;
    }
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex < 0 ||
        static_cast<qsizetype>(profileIndex) >= m_profiles.profiles().size()) {
        *error = QStringLiteral("selected profile disappeared during surface reconciliation");
        return false;
    }

    const auto &profile = m_profiles.profiles().at(profileIndex);
    QVector<ShellLayout::LogicalOutput> selectedOutputs = inventory.outputs;
    const ShellVisibility::CompositorVisibilitySnapshot *visibilitySnapshot = nullptr;
    if (m_visibilityClient && !m_visibilityClient->safeVisibleRequired() &&
        m_visibilityClient->snapshot()) {
        const auto compositorOutputs = logicalOutputs(*m_visibilityClient->snapshot());
        const auto outputMatch = ShellOrchestration::OutputInventoryMatcher::match(
            compositorOutputs, inventory.outputs);
        if (outputMatch.ok()) {
            selectedOutputs = compositorOutputs;
            visibilitySnapshot = &*m_visibilityClient->snapshot();
        } else {
            // AGENT-GUARD: Qt and compositor output generations can cross
            // during hotplug. A mixed generation is never evaluated; keeping
            // every panel visible is the fail-safe policy until they converge.
            qWarning().noquote()
                << "QindaQt shell is using safe-visible output fallback:"
                << outputMatch.message;
        }
    }

    const auto layout = ShellLayout::PanelLayoutSolver::solve(profile.panels,
                                                               selectedOutputs);
    if (!layout.ok()) {
        *error = layout.error.message;
        return false;
    }
    if (!m_controller) {
        *error = QStringLiteral("panel surface controller is not initialized");
        return false;
    }
    const auto basePlan = ShellSurface::PanelSurfaceConfigurationPlanner::plan(layout);
    if (!basePlan.ok()) {
        *error = basePlan.error.message;
        return false;
    }
    if (!m_interactions) {
        *error = QStringLiteral("panel interaction store is not initialized");
        return false;
    }
    QVector<ShellVisibility::PanelSurfaceIdentity> identities;
    identities.reserve(basePlan.surfaces.size());
    for (const auto &surface : basePlan.surfaces) {
        identities.append({surface.identity.panelId, surface.identity.outputId});
    }
    QString interactionError;
    if (!m_interactions->setIdentities(identities, &interactionError)) {
        *error = std::move(interactionError);
        return false;
    }

    ShellOrchestration::PanelRuntimeAssemblyResult runtime;
    if (visibilitySnapshot != nullptr) {
        const auto visibility =
            ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                profile, layout, *visibilitySnapshot, m_interactions->snapshot());
        if (visibility.ok()) {
            runtime = ShellOrchestration::PanelRuntimePlanAssembler::fromEvaluation(
                basePlan, visibility.evaluation);
        } else {
            qWarning().noquote()
                << "QindaQt shell rejected live visibility and kept panels visible:"
                << visibility.error.message;
            runtime = ShellOrchestration::PanelRuntimePlanAssembler::safeVisible(basePlan);
        }
    } else {
        runtime = ShellOrchestration::PanelRuntimePlanAssembler::safeVisible(basePlan);
    }
    if (!runtime.ok()) {
        *error = runtime.error.message;
        return false;
    }
    const auto result = m_controller->reconcilePlan(std::move(runtime.plan));
    if (!result.ok()) {
        *error = result.message;
        return false;
    }
    if (m_notificationWindows &&
        !m_notificationWindows->reconcile(m_application.primaryScreen(), error)) {
        return false;
    }
    return true;
}

void ShellRuntimeApplication::attachOutputSignals(QScreen *screen)
{
    if (screen == nullptr) {
        return;
    }
    const auto schedule = [this] { scheduleOutputReconcile(); };
    connect(screen, &QScreen::geometryChanged, this, schedule);
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, schedule);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, schedule);
    connect(screen, &QScreen::orientationChanged, this, schedule);
}

void ShellRuntimeApplication::scheduleOutputReconcile()
{
    m_outputDebounce.start();
}

void ShellRuntimeApplication::resetRuntime()
{
    m_outputDebounce.stop();
    m_notificationCenterShortcut.reset();
    m_globalShortcutRegistrar.reset();
    m_notificationWindows.reset();
    m_interactions.reset();
    m_visibilityClient.reset();
    m_visibilityTransport.reset();
    m_controller.reset();
    m_backend.reset();
    m_windowFactory.reset();
    m_notificationCenterAccess.reset();
    m_notificationPresentation.reset();
    m_notificationClient.reset();
    m_notificationTransport.reset();
}

} // namespace QindaQt::Shell
