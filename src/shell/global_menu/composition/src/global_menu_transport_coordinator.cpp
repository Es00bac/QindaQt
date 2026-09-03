// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h>

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>

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
    connect(&m_registry, &Registrar::RegistrarRegistry::windowRegistered, this,
            [this](const Registrar::AppMenuRegistration &) { refreshFocus(); });
    connect(&m_registry, &Registrar::RegistrarRegistry::windowUnregistered, this,
            [this](quint32, const QString &) { refreshFocus(); });
    connect(&m_applet, &GlobalMenuAppletAccess::activationRequested, this,
            &GlobalMenuTransportCoordinator::activate);
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
    const std::optional<quint32> registrarWindowId =
        m_windowIdSource.registrarWindowIdFor(focus->window);
    if (!registrarWindowId) {
        clearAuthority();
        return;
    }
    const std::optional<Registrar::AppMenuRegistration> registration =
        m_registry.registrationFor(*registrarWindowId);
    if (!registration) {
        clearAuthority();
        return;
    }
    if (m_client && m_registrationGeneration == registration->registrationGeneration
        && m_focusGeneration == focus->focusGeneration && m_selector.current()) {
        return;
    }
    bindRegistration(*focus, *registration);
}

void GlobalMenuTransportCoordinator::bindRegistration(
    const Ownership::ActiveWindowObservation &focus,
    const Registrar::AppMenuRegistration &registration)
{
    const Ownership::MenuProviderRegistration claim{
        .windowId = focus.window.windowId,
        .providerUniqueName = registration.ownerUniqueName,
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
    m_registrationGeneration = registration.registrationGeneration;
    m_focusGeneration = focus.focusGeneration;
    m_client = std::make_unique<DbusMenu::DbusMenuClient>(
        m_connection, registration.ownerUniqueName, registration.menuObjectPath,
        focus.window.windowId);
    m_exporter = std::make_unique<Exporter::MenuExporter>(*m_client, *this);
    connect(m_client.get(), &DbusMenu::DbusMenuClient::treeChanged, this,
            &GlobalMenuTransportCoordinator::publishClientTree);
    connect(m_client.get(), &DbusMenu::DbusMenuClient::unavailable, this,
            &GlobalMenuTransportCoordinator::clearAuthority, Qt::QueuedConnection);
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
    const std::optional<quint32> registrarWindowId = focus
        ? m_windowIdSource.registrarWindowIdFor(focus->window)
        : std::nullopt;
    const std::optional<Registrar::AppMenuRegistration> registration = registrarWindowId
        ? m_registry.registrationFor(*registrarWindowId)
        : std::nullopt;
    if (!focus || !registration
        || registration->registrationGeneration != m_registrationGeneration
        || focus->focusGeneration != m_focusGeneration) {
        clearAuthority();
        return;
    }
    const Ownership::AuthenticationResult authentication = m_authenticator.authenticate(
        Ownership::MenuProviderRegistration{.windowId = focus->window.windowId,
                                            .providerUniqueName = registration->ownerUniqueName,
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
    if (m_client) {
        m_client->stop();
    }
    m_exporter.reset();
    m_client.reset();
    m_selector.clear();
    m_registrationGeneration = 0;
    m_focusGeneration = 0;
    m_applet.publishUnavailable();
}

void GlobalMenuTransportCoordinator::stop()
{
    clearAuthority();
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
