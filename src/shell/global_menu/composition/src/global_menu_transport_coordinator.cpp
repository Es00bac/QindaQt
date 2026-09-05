// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h>

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusServiceWatcher>

#include <utility>

namespace QindaQt::Shell::GlobalMenu::Composition
{

GlobalMenuTransportCoordinator::GlobalMenuTransportCoordinator(
    QDBusConnection connection, const Ownership::ActiveWindowSource &activeWindowSource,
    const RegistrarWindowIdSource &windowIdSource, Registrar::RegistrarRegistry &registry,
    GlobalMenuAppletAccess &applet, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_activeWindowSource(activeWindowSource)
    , m_windowIdSource(windowIdSource)
    , m_registry(registry)
    , m_applet(applet)
    , m_credentials(m_connection)
    , m_authenticator(m_activeWindowSource, m_credentials)
{
    m_announcedServiceWatcher = new QDBusServiceWatcher(
        {}, m_connection, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_announcedServiceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this](const QString &service, const QString &, const QString &) {
                if (service == m_watchedAnnouncedService) {
                    refreshFocus();
                }
            });
    connect(&m_registry, &Registrar::RegistrarRegistry::windowRegistered, this,
            [this](const Registrar::AppMenuRegistration &) { refreshFocus(); });
    connect(&m_registry, &Registrar::RegistrarRegistry::windowUnregistered, this,
            [this](quint32, const QString &) { refreshFocus(); });
    connect(&m_applet, &GlobalMenuAppletAccess::activationRequested, this,
            &GlobalMenuTransportCoordinator::activate);
    connect(&m_applet, &GlobalMenuAppletAccess::rendererPresentChanged, this,
            &GlobalMenuTransportCoordinator::refreshHostedMenu);
}

GlobalMenuTransportCoordinator::GlobalMenuTransportCoordinator(
    QDBusConnection connection, const Ownership::ActiveWindowSource &activeWindowSource,
    const RegistrarWindowIdSource &windowIdSource,
    const AnnouncedMenuAddressSource &announcedMenuSource,
    Registrar::RegistrarRegistry &registry, GlobalMenuAppletAccess &applet,
    QObject *parent)
    : GlobalMenuTransportCoordinator(std::move(connection), activeWindowSource,
                                     windowIdSource, registry, applet, parent)
{
    m_announcedMenuSource = &announcedMenuSource;
}

GlobalMenuTransportCoordinator::~GlobalMenuTransportCoordinator()
{
    stop();
}

void GlobalMenuTransportCoordinator::refreshFocus()
{
    const std::optional<Ownership::ActiveWindowObservation> focus =
        m_activeWindowSource.activeWindow();
    if (!focus || !focus->window.isValid()) {
        clearAuthority();
        return;
    }
    m_selector.applyFocusGeneration(focus->focusGeneration);
    const std::optional<ProviderEndpoint> endpoint = endpointFor(*focus);
    if (!endpoint) {
        clearAuthority();
        return;
    }
    if (m_client && m_boundEndpoint == *endpoint
        && m_focusGeneration == focus->focusGeneration && m_selector.current()) {
        return;
    }
    bindRegistration(*focus, *endpoint);
}

std::optional<GlobalMenuTransportCoordinator::ProviderEndpoint>
GlobalMenuTransportCoordinator::endpointFor(
    const Ownership::ActiveWindowObservation &focus)
{
    const std::optional<quint32> registrarWindowId =
        m_windowIdSource.registrarWindowIdFor(focus.window);
    if (registrarWindowId) {
        watchAnnouncedService({});
        const auto registration = m_registry.registrationFor(*registrarWindowId);
        if (!registration) {
            return std::nullopt;
        }
        return ProviderEndpoint{.uniqueOwner = registration->ownerUniqueName,
                                .objectPath = registration->menuObjectPath.path(),
                                .announcedService = {},
                                .registrationGeneration =
                                    registration->registrationGeneration};
    }
    if (m_announcedMenuSource == nullptr) {
        watchAnnouncedService({});
        return std::nullopt;
    }
    const auto announced = m_announcedMenuSource->announcedMenuFor(focus.window);
    if (!announced || announced->serviceName.isEmpty()
        || announced->objectPath.isEmpty()) {
        watchAnnouncedService({});
        return std::nullopt;
    }
    watchAnnouncedService(announced->serviceName);
    if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
        return std::nullopt;
    }
    const QDBusReply<QString> owner =
        m_connection.interface()->serviceOwner(announced->serviceName);
    if (!owner.isValid() || owner.value().isEmpty()) {
        return std::nullopt;
    }
    return ProviderEndpoint{.uniqueOwner = owner.value(),
                            .objectPath = announced->objectPath,
                            .announcedService = announced->serviceName,
                            .registrationGeneration = 0};
}

void GlobalMenuTransportCoordinator::watchAnnouncedService(
    const QString &serviceName)
{
    if (serviceName == m_watchedAnnouncedService) {
        return;
    }
    if (!m_watchedAnnouncedService.isEmpty()) {
        m_announcedServiceWatcher->removeWatchedService(
            m_watchedAnnouncedService);
    }
    m_watchedAnnouncedService = serviceName;
    if (!m_watchedAnnouncedService.isEmpty()) {
        m_announcedServiceWatcher->addWatchedService(
            m_watchedAnnouncedService);
    }
}

void GlobalMenuTransportCoordinator::bindRegistration(
    const Ownership::ActiveWindowObservation &focus,
    const ProviderEndpoint &endpoint)
{
    // A focus switch does not revoke an endpoint that this live renderer has
    // already proved it can serve. Keeping that acknowledgment prevents the
    // inactive window's content geometry from jumping as focus moves.
    m_hosted = false;
    const Ownership::MenuProviderRegistration claim{
        .windowId = focus.window.windowId,
        .providerUniqueName = endpoint.uniqueOwner,
        .claimedProcessId = focus.window.processId};
    const Ownership::AuthenticationResult authentication = m_authenticator.authenticate(claim);
    if (!authentication.accepted || !authentication.proof) {
        clearAuthority();
        return;
    }

    // Retire only the old transport. Keeping the selector until adopt() lets
    // the G0 authority preserve the epoch when the same focused window moves
    // to a replacement registrar path; adopt still mints a new revision.
    if (m_client) {
        m_client->stop();
    }
    m_exporter.reset();
    m_client.reset();
    m_applet.publishUnavailable();
    m_selector.adopt(*authentication.proof);
    m_boundEndpoint = endpoint;
    m_focusGeneration = focus.focusGeneration;
    m_client = std::make_unique<DbusMenu::DbusMenuClient>(
        m_connection, endpoint.uniqueOwner, QDBusObjectPath(endpoint.objectPath),
        focus.window.windowId);
    m_exporter = std::make_unique<Exporter::MenuExporter>(*m_client, *this);
    connect(m_client.get(), &DbusMenu::DbusMenuClient::treeChanged, this,
            &GlobalMenuTransportCoordinator::publishClientTree);
    connect(m_client.get(), &DbusMenu::DbusMenuClient::unavailable, this,
            [this] {
                clearAuthority();
                refreshFocus();
            }, Qt::QueuedConnection);
    QString error;
    if (!m_client->start(&error)) {
        clearAuthority();
    }
}

void GlobalMenuTransportCoordinator::publishClientTree()
{
    if (!m_client || !m_exporter) {
        return;
    }
    const std::optional<Ownership::ActiveWindowObservation> focus =
        m_activeWindowSource.activeWindow();
    const std::optional<ProviderEndpoint> endpoint = focus
        ? endpointFor(*focus) : std::nullopt;
    if (!focus || !endpoint || *endpoint != m_boundEndpoint
        || focus->focusGeneration != m_focusGeneration) {
        clearAuthority();
        return;
    }
    const Ownership::AuthenticationResult authentication = m_authenticator.authenticate(
        Ownership::MenuProviderRegistration{.windowId = focus->window.windowId,
                                            .providerUniqueName = endpoint->uniqueOwner,
                                            .claimedProcessId = focus->window.processId});
    if (!authentication.accepted || !authentication.proof) {
        clearAuthority();
        return;
    }
    // AGENT-GUARD: each newly accepted remote layout is re-authenticated and
    // receives a fresh selector revision before the unchanged exporter stamps
    // it. The untrusted remote revision never becomes invocation authority.
    m_selector.adopt(*authentication.proof);
    const Exporter::ExportResult exported = m_exporter->refresh();
    if (exported.outcome == Exporter::ExportOutcome::Published
        || exported.outcome == Exporter::ExportOutcome::Unchanged) {
        const std::optional<Protocol::MenuTree> tree = m_exporter->lastAccepted();
        if (tree) {
            m_applet.publishTree(*tree);
            refreshHostedMenu();
        }
    }
}

void GlobalMenuTransportCoordinator::activate(const QString &actionId)
{
    if (!m_client || !m_exporter) {
        Q_EMIT activationRejected(QStringLiteral("no-active-provider"));
        return;
    }
    // Capture the exact facade tree synchronously with the request. The guard
    // then compares it to the selector before any wire Event is admitted.
    const std::optional<Protocol::MenuTree> tree = m_exporter->lastAccepted();
    if (!tree) {
        Q_EMIT activationRejected(QStringLiteral("no-active-provider"));
        return;
    }
    const Ownership::InvocationResult result = Ownership::InvocationGuard::evaluate(
        m_selector, *tree,
        Ownership::InvocationRequest{.windowId = tree->ownerWindowId,
                                     .epoch = tree->epoch,
                                     .revision = tree->revision,
                                     .actionId = actionId});
    if (!result.accepted) {
        Q_EMIT activationRejected(result.reasonCode);
        return;
    }
    bool converted = false;
    const qint32 itemId = actionId.toInt(&converted);
    if (!converted || itemId <= 0 || QString::number(itemId) != actionId) {
        Q_EMIT activationRejected(QStringLiteral("invalid-transport-action-id"));
        return;
    }
    // One applet signal is one intent and produces exactly one Event call.
    // DbusMenuClient deliberately never retries an uncertain result.
    m_client->sendEvent(itemId, QStringLiteral("clicked"));
}

void GlobalMenuTransportCoordinator::clearAuthority()
{
    m_hosted = false;
    if (m_client) {
        m_client->stop();
    }
    m_exporter.reset();
    m_client.reset();
    m_selector.clear();
    m_boundEndpoint = {};
    m_focusGeneration = 0;
    m_applet.publishUnavailable();
}

void GlobalMenuTransportCoordinator::refreshHostedMenu()
{
    const bool shouldHost = m_applet.rendererPresent() && m_applet.available()
        && m_exporter && m_exporter->lastAccepted().has_value()
        && !m_boundEndpoint.uniqueOwner.isEmpty()
        && !m_boundEndpoint.objectPath.isEmpty();
    if (shouldHost == m_hosted) {
        return;
    }
    m_hosted = shouldHost;
    Q_EMIT hostedMenuChanged(m_boundEndpoint.uniqueOwner,
                             m_boundEndpoint.objectPath, m_hosted);
}

void GlobalMenuTransportCoordinator::stop()
{
    clearAuthority();
    watchAnnouncedService({});
}

std::optional<Exporter::ExportLineage> GlobalMenuTransportCoordinator::lineageFor(
    const QUuid &ownerWindowId) const
{
    const std::optional<Ownership::SelectedProvider> current = m_selector.current();
    if (!current || current->window.windowId != ownerWindowId) {
        return std::nullopt;
    }
    return Exporter::ExportLineage{.epoch = current->epoch,
                                   .revision = current->revision};
}

std::optional<Protocol::MenuTree> GlobalMenuTransportCoordinator::publishedTree() const
{
    return m_exporter ? m_exporter->lastAccepted() : std::nullopt;
}

} // namespace QindaQt::Shell::GlobalMenu::Composition
