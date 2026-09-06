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

namespace
{

// Bounded window in which a withdrawn authority may re-prove itself before the
// retained placeholder presentation is cleared. Must comfortably exceed one
// compositor invalidation -> identity reread D-Bus round trip so transient
// churn never collapses the panel; short enough that a genuinely menu-less
// focus still clears the old menu promptly.
constexpr int kPresentationGraceMilliseconds = 500;

} // namespace

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
            this, [this](const QString &service, const QString &oldOwner,
                         const QString &newOwner) {
                if (service == m_watchedAnnouncedService) {
                    if (!m_boundEndpoint.announcedService.isEmpty()
                        && m_boundEndpoint.announcedService == service
                        && oldOwner == m_boundEndpoint.uniqueOwner
                        && newOwner != oldOwner) {
                        withdrawHostedMenu(m_boundEndpoint);
                    }
                    refreshFocus();
                }
            });
    connect(&m_registry, &Registrar::RegistrarRegistry::windowRegistered, this,
            [this](const Registrar::AppMenuRegistration &registration) {
                if (!m_boundEndpoint.uniqueOwner.isEmpty()
                    && registration.ownerUniqueName == m_boundEndpoint.uniqueOwner
                    && registration.menuObjectPath.path() != m_boundEndpoint.objectPath) {
                    withdrawHostedMenu(m_boundEndpoint);
                }
                refreshFocus();
            });
    connect(&m_registry, &Registrar::RegistrarRegistry::windowUnregistered, this,
            [this](quint32, const QString &owner) {
                if (owner == m_boundEndpoint.uniqueOwner) {
                    withdrawHostedMenu(m_boundEndpoint);
                }
                refreshFocus();
            });
    connect(&m_applet, &GlobalMenuAppletAccess::activationRequested, this,
            &GlobalMenuTransportCoordinator::activate);
    connect(&m_applet, &GlobalMenuAppletAccess::rendererPresentChanged, this,
            &GlobalMenuTransportCoordinator::refreshHostedMenu);
    m_presentationGraceTimer.setSingleShot(true);
    m_presentationGraceTimer.setInterval(kPresentationGraceMilliseconds);
    connect(&m_presentationGraceTimer, &QTimer::timeout, this,
            &GlobalMenuTransportCoordinator::expirePresentationGrace);
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
        // No authenticated observation: revoke invocation authority at once,
        // but give the identity reread one grace window to re-prove the same
        // provider before the retained presentation is cleared.
        suspendAuthority();
        return;
    }
    const std::optional<ProviderEndpoint> endpoint = endpointFor(*focus);
    if (!endpoint) {
        suspendAuthority();
        return;
    }
    const bool sameProvider = m_client && m_boundWindow == focus->window
        && m_boundEndpoint == *endpoint;
    if (sameProvider && !m_authoritySuspended
        && m_focusGeneration == focus->focusGeneration && m_selector.current()) {
        return;
    }
    if (sameProvider) {
        // The same window and endpoint still own focus; only the observation
        // generation moved (a visibility change that is not a focus move, or a
        // transient invalidation/reread cycle). Renew lineage in place instead
        // of tearing the binding down and flashing the panel. The selector's
        // focus-generation fence is deliberately not applied on this path:
        // renewal re-authenticates against the moved generation before
        // adopting, so the same window keeps its epoch.
        renewBoundProvider(*focus);
        return;
    }
    m_selector.applyFocusGeneration(focus->focusGeneration);
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
    m_presentationGraceTimer.stop();
    m_authoritySuspended = false;
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
        // A previously acknowledged endpoint may be revisited after focus
        // moved away. Failed authentication revokes that exact cached proof;
        // it must not leave the application's local menu suppressed.
        withdrawHostedMenu(endpoint);
        if (m_lastHostedWindowId == focus.window.windowId
            && m_lastHostedEndpoint != endpoint) {
            withdrawHostedMenu(m_lastHostedEndpoint);
        }
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
    // AGENT-GUARD: the previous provider's projection stays painted as an
    // inert placeholder until the replacement's first tree arrives. Publishing
    // unavailable here would collapse the panel slot to zero extent and
    // reflow the panel row on every focus switch — the visible flash. The
    // facade drops `available` during the transition, so retained entries are
    // never actionable for the new focus. If the replacement's first layout
    // fetch is rejected outright, the initialLayoutFailed handler below clears
    // the placeholder instead of holding it open.
    m_applet.beginTransition();
    m_selector.adopt(*authentication.proof);
    m_boundEndpoint = endpoint;
    m_boundWindow = focus.window;
    m_focusGeneration = focus.focusGeneration;
    m_client = std::make_unique<DbusMenu::DbusMenuClient>(
        m_connection, endpoint.uniqueOwner, QDBusObjectPath(endpoint.objectPath),
        focus.window.windowId, 2'000, this);
    m_exporter = std::make_unique<Exporter::MenuExporter>(*m_client, *this);
    const quint64 clientGeneration = ++m_clientGeneration;
    connect(m_client.get(), &DbusMenu::DbusMenuClient::treeChanged, this,
            &GlobalMenuTransportCoordinator::publishClientTree);
    connect(m_client.get(), &DbusMenu::DbusMenuClient::unavailable, this,
            [this, clientGeneration, endpoint] {
                if (clientGeneration != m_clientGeneration || !m_client
                    || m_boundEndpoint != endpoint) {
                    return;
                }
                withdrawHostedMenu(endpoint);
                clearAuthority();
                refreshFocus();
            }, Qt::QueuedConnection);
    connect(m_client.get(), &DbusMenu::DbusMenuClient::initialLayoutFailed, this,
            [this, clientGeneration, endpoint] {
                if (clientGeneration != m_clientGeneration || !m_client
                    || m_boundEndpoint != endpoint) {
                    return;
                }
                // AGENT-GUARD: the replacement endpoint never delivered a menu
                // (its first GetLayout errored or failed to decode), so the
                // retained projection from the previous provider would
                // otherwise hang in `loading` indefinitely. Clear fail-closed
                // immediately — no grace window and no refreshFocus() rebind,
                // so a registrar entry that names an unserved path cannot spin
                // GetLayout; only a fresh registrar/focus signal rebinds.
                withdrawHostedMenu(endpoint);
                clearAuthority();
            }, Qt::QueuedConnection);
    QString error;
    if (!m_client->start(&error)) {
        withdrawHostedMenu(m_boundEndpoint);
        clearAuthority();
    }
}

void GlobalMenuTransportCoordinator::publishClientTree()
{
    if (!m_client || !m_exporter) {
        return;
    }
    if (m_authoritySuspended) {
        // A treeChanged during suspension means the kept client's fetch
        // completed; route through the focus logic so a matching observation
        // resumes and publishes it instead of dropping the event.
        refreshFocus();
        return;
    }
    const std::optional<Ownership::ActiveWindowObservation> focus =
        m_activeWindowSource.activeWindow();
    const std::optional<ProviderEndpoint> endpoint = focus
        ? endpointFor(*focus) : std::nullopt;
    if (!focus || !endpoint || *endpoint != m_boundEndpoint
        || focus->window != m_boundWindow
        || focus->focusGeneration != m_focusGeneration) {
        suspendAuthority();
        return;
    }
    const Ownership::AuthenticationResult authentication = m_authenticator.authenticate(
        Ownership::MenuProviderRegistration{.windowId = focus->window.windowId,
                                            .providerUniqueName = endpoint->uniqueOwner,
                                            .claimedProcessId = focus->window.processId});
    if (!authentication.accepted || !authentication.proof) {
        withdrawHostedMenu(m_boundEndpoint);
        clearAuthority();
        return;
    }
    // AGENT-GUARD: each newly accepted remote layout is re-authenticated and
    // receives a fresh selector revision before the unchanged exporter stamps
    // it. The untrusted remote revision never becomes invocation authority.
    m_selector.adopt(*authentication.proof);
    const Exporter::ExportResult exported = m_exporter->refresh();
    if (exported.outcome == Exporter::ExportOutcome::Published) {
        const std::optional<Protocol::MenuTree> tree = m_exporter->lastAccepted();
        if (tree) {
            m_applet.publishTree(*tree);
            refreshHostedMenu();
        }
    } else if (exported.outcome == Exporter::ExportOutcome::Unchanged) {
        // Content-identical restamp: the facade already projects exactly this
        // tree. Republishing would bump the publication generation and rebuild
        // every delegate for no visible change; the selector/exporter lineage
        // advance above is sufficient for invocation coherence.
        refreshHostedMenu();
    } else {
        withdrawHostedMenu(m_boundEndpoint);
        clearAuthority();
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
    m_presentationGraceTimer.stop();
    m_authoritySuspended = false;
    m_hosted = false;
    ++m_clientGeneration;
    if (m_client) {
        m_client->stop();
    }
    m_exporter.reset();
    m_client.reset();
    m_selector.clear();
    m_boundEndpoint = {};
    m_boundWindow = {};
    m_focusGeneration = 0;
    m_applet.publishUnavailable();
}

void GlobalMenuTransportCoordinator::suspendAuthority()
{
    if (m_authoritySuspended) {
        // Keep the original grace deadline; repeated invalidations must not
        // extend the retained placeholder indefinitely.
        return;
    }
    if (m_applet.items().isEmpty()) {
        // No presentation is retained, so there is nothing to grace: this is
        // the ordinary unavailable transition.
        clearAuthority();
        return;
    }
    // Invocation authority dies now: with the selector cleared the invocation
    // guard can never match, so no dbusmenu Event crosses while the identity
    // reread is uncertain. The facade is deliberately NOT transitioned: the
    // compositor invalidates on every visibility-affecting change (including
    // the menu's own popup opening), and dropping `available` for each
    // sub-second withdraw/reread cycle closes the open popup and disables the
    // delegates between press and release — the user-visible "menu items are
    // dead" defect. Presentation and interactivity are retained; only a
    // reread that proves a DIFFERENT window/endpoint fences the retained
    // entries (bindRegistration opens an inert transition), and grace expiry
    // publishes the truthful unavailable state. The client and exporter stay
    // alive so a prompt reread of the same window resumes without a GetLayout
    // round trip or a delegate rebuild.
    m_authoritySuspended = true;
    m_selector.clear();
    m_focusGeneration = 0;
    m_presentationGraceTimer.start();
}

void GlobalMenuTransportCoordinator::expirePresentationGrace()
{
    if (!m_authoritySuspended) {
        return;
    }
    // No provider re-proved the retained presentation within the grace window:
    // the focus genuinely has no usable menu. Retire the kept transport and
    // publish the truthful unavailable state. The hosted acknowledgment stays
    // retained per endpoint while the renderer lives, exactly as
    // clearAuthority() leaves it, so the inactive application's window
    // geometry does not jump.
    m_authoritySuspended = false;
    m_hosted = false;
    ++m_clientGeneration;
    if (m_client) {
        m_client->stop();
    }
    m_exporter.reset();
    m_client.reset();
    m_boundEndpoint = {};
    m_boundWindow = {};
    m_applet.publishUnavailable();
}

void GlobalMenuTransportCoordinator::renewBoundProvider(
    const Ownership::ActiveWindowObservation &focus)
{
    m_presentationGraceTimer.stop();
    const bool wasSuspended = m_authoritySuspended;
    m_authoritySuspended = false;
    const Ownership::MenuProviderRegistration claim{
        .windowId = focus.window.windowId,
        .providerUniqueName = m_boundEndpoint.uniqueOwner,
        .claimedProcessId = focus.window.processId};
    const Ownership::AuthenticationResult authentication =
        m_authenticator.authenticate(claim);
    if (!authentication.accepted || !authentication.proof) {
        // The retained placeholder belonged to a provider that can no longer
        // prove itself against the current focus; this is a hard revocation,
        // not a transient gap, so the presentation clears immediately.
        withdrawHostedMenu(m_boundEndpoint);
        clearAuthority();
        return;
    }
    m_selector.adopt(*authentication.proof);
    m_focusGeneration = focus.focusGeneration;
    if (!m_exporter || !m_exporter->lastAccepted().has_value()) {
        // The first layout fetch is still in flight; the client's treeChanged
        // completes publication through publishClientTree(). Keep the original
        // grace deadline armed so a fetch that never completes cannot hold the
        // placeholder open indefinitely.
        if (wasSuspended) {
            m_presentationGraceTimer.start();
        }
        return;
    }
    const Exporter::ExportResult exported = m_exporter->refresh();
    if (exported.outcome == Exporter::ExportOutcome::Published) {
        m_applet.publishTree(*m_exporter->lastAccepted());
        refreshHostedMenu();
    } else if (exported.outcome == Exporter::ExportOutcome::Unchanged) {
        // Content-identical restamp: the facade never left the ready state for
        // a transient withdrawal, so there is nothing to restore — no
        // projection signal, no delegate rebuild, and an open popup survives
        // the churn. The selector/exporter lineage advance above is
        // sufficient for invocation coherence.
        refreshHostedMenu();
    } else {
        withdrawHostedMenu(m_boundEndpoint);
        clearAuthority();
    }
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
    if (m_hosted) {
        const auto focus = m_activeWindowSource.activeWindow();
        if (focus) {
            m_lastHostedWindowId = focus->window.windowId;
            m_lastHostedEndpoint = m_boundEndpoint;
        }
    }
    Q_EMIT hostedMenuChanged(m_boundEndpoint.uniqueOwner,
                             m_boundEndpoint.objectPath, m_hosted);
}

void GlobalMenuTransportCoordinator::withdrawHostedMenu(
    const ProviderEndpoint &endpoint)
{
    if (endpoint.uniqueOwner.isEmpty() || endpoint.objectPath.isEmpty()) {
        return;
    }
    Q_EMIT hostedMenuChanged(endpoint.uniqueOwner, endpoint.objectPath, false);
    if (endpoint == m_boundEndpoint) {
        m_hosted = false;
    }
    if (endpoint == m_lastHostedEndpoint) {
        m_lastHostedEndpoint = {};
        m_lastHostedWindowId = {};
    }
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
