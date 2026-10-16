// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinshellwindowactions.h"

#include "kwinhybridsession.h"
#include "kwinshellwindowidentity.h"
#include "kwinshelltaskfacts.h"
#include "kwinshellvisibilitypublisher.h"
#include "managedwindowregistry.h"

#include <wayland/clientconnection.h>
#include <wayland/layershell_v1.h>
#include <wayland/surface.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>

#include "qindaqt/compositor/shellcredentialcache.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>
#include <QSet>
#include <QVariantMap>

#include <limits>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto PanelScope = "dock";
constexpr auto ProcessIdCredential = "ProcessID";
constexpr auto ShellObjectPath = "/org/qindaqt/CompositorShell";
constexpr auto ShellInterface = "org.qindaqt.CompositorShell1";
constexpr auto IdentityChangedSignal = "ActiveWindowIdentityChanged";
constexpr auto TaskFactsChangedSignal = "TaskListSnapshotChanged";

} // namespace

QtBusShellCredentialSource::QtBusShellCredentialSource(QDBusConnection connection)
    : m_connection(std::move(connection))
    , m_ownerWatch(std::make_unique<QDBusServiceWatcher>())
{
    m_ownerWatch->setConnection(m_connection);
    m_ownerWatch->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    m_cache = std::make_unique<ShellCredentialCache>(
        [this](const QString &uniqueName) { return lookup(uniqueName); },
        [this](const QString &uniqueName, bool watched) {
            if (watched) {
                m_ownerWatch->addWatchedService(uniqueName);
            } else {
                m_ownerWatch->removeWatchedService(uniqueName);
            }
        });
    // AGENT-CONTRACT: unique names are never reused by the bus daemon, so the
    // only event that can make a cached PID wrong is the owner disconnecting,
    // which NameOwnerChanged reports here.
    QObject::connect(m_ownerWatch.get(), &QDBusServiceWatcher::serviceUnregistered,
                     m_ownerWatch.get(), [this](const QString &uniqueName) {
                         m_cache->forget(uniqueName);
                     });
}

QtBusShellCredentialSource::~QtBusShellCredentialSource() = default;

std::optional<qint64> QtBusShellCredentialSource::processIdForUniqueName(
    const QString &uniqueName) const
{
    return m_cache->processIdForUniqueName(uniqueName);
}

std::optional<qint64> QtBusShellCredentialSource::lookup(const QString &uniqueName) const
{
    if (!m_connection.isConnected() || uniqueName.isEmpty()
        || !uniqueName.startsWith(u':')) {
        return std::nullopt;
    }
    auto request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetConnectionCredentials"));
    request << uniqueName;
    // AGENT-GUARD: QDBus::Block, never BlockWithGui: a nested event loop on
    // the compositor thread would re-enter KWin. The explicit timeout bounds
    // the stall when the bus daemon is slow.
    const QDBusMessage reply =
        m_connection.call(request, QDBus::Block, LookupTimeoutMilliseconds);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return std::nullopt;
    }
    const auto credentials = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
    bool ok = false;
    const qulonglong processId = credentials
        .value(QString::fromLatin1(ProcessIdCredential)).toULongLong(&ok);
    if (!ok || processId <= 1
        || processId > static_cast<qulonglong>(std::numeric_limits<qint64>::max())) {
        return std::nullopt;
    }
    return static_cast<qint64>(processId);
}

KWinShellPanelOwnerSource::KWinShellPanelOwnerSource(QObject *parent)
    : QObject(parent)
{
    auto *const server = KWin::waylandServer();
    auto *const layerShell = server
        ? server->findChild<KWin::LayerShellV1Interface *>() : nullptr;
    if (layerShell) {
        connect(layerShell, &KWin::LayerShellV1Interface::surfaceCreated,
                this, &KWinShellPanelOwnerSource::track);
    }
}

void KWinShellPanelOwnerSource::track(KWin::LayerSurfaceV1Interface *surface)
{
    if (!surface) {
        return;
    }
    // AGENT-GUARD: surfaceCreated precedes the client's first committed
    // set_scope request. Retain every layer role here and filter committed
    // dock roles at the authorization decision; filtering at creation would
    // permanently miss the real production panels.
    m_surfaces.append(surface);
    if (surface->surface()) {
        connect(surface->surface(), &KWin::SurfaceInterface::committed,
                this, &KWinShellPanelOwnerSource::refreshPanelOwner);
    }
    if (auto *waylandSurface = surface->surface()) {
        connect(waylandSurface, &KWin::SurfaceInterface::aboutToBeDestroyed,
                this, [this, surface] {
                    m_surfaces.removeAll(surface);
                    refreshPanelOwner();
                });
        if (auto *client = waylandSurface->client()) {
            // AGENT-GUARD: KWin marks tearingDown only AFTER this signal.
            // Remove all this client's roles before publishing revocation;
            // otherwise synchronous authorization could still admit its PID.
            connect(client, &KWin::ClientConnection::aboutToBeDestroyed,
                    this, &KWinShellPanelOwnerSource::retireClient,
                    Qt::UniqueConnection);
        }
    }
    connect(surface, &KWin::LayerSurfaceV1Interface::aboutToBeDestroyed,
            this, [this, surface] {
                if (surface->surface()) {
                    disconnect(surface->surface(), nullptr, this, nullptr);
                }
                m_surfaces.removeAll(surface);
                refreshPanelOwner();
            });
}

void KWinShellPanelOwnerSource::retireClient()
{
    const auto *client = qobject_cast<KWin::ClientConnection *>(sender());
    m_surfaces.removeIf([client](const auto &tracked) {
        return !tracked || !tracked->surface()
            || tracked->surface()->client() == client;
    });
    refreshPanelOwner();
}

void KWinShellPanelOwnerSource::refreshPanelOwner()
{
    const auto owner = shellPanelProcessId();
    // AGENT-GUARD: A panel commits on every animation frame. Publishing an
    // unchanged authority makes the shell reread and redraw, creating a
    // compositor/shell feedback loop. Authorization still scans live roles;
    // this retained value suppresses only redundant change notifications.
    if (owner == m_lastPublishedOwner) {
        return;
    }
    m_lastPublishedOwner = owner;
    Q_EMIT shellPanelOwnerChanged();
}

std::optional<qint64> KWinShellPanelOwnerSource::shellPanelProcessId() const
{
    QSet<qint64> owners;
    for (const auto &tracked : m_surfaces) {
        const auto *surface = tracked.data();
        if (!surface || surface->scope() != QLatin1StringView(PanelScope)) {
            continue;
        }
        auto *client = surface && surface->surface()
            ? surface->surface()->client() : nullptr;
        if (!surface || !surface->isCommitted() || !client || client->tearingDown()) {
            continue;
        }
        const qint64 processId = static_cast<qint64>(client->processId());
        if (processId <= 1) {
            return std::nullopt;
        }
        owners.insert(processId);
        if (owners.size() > 1) {
            return std::nullopt;
        }
    }
    return owners.size() == 1
        ? std::optional<qint64>(*owners.cbegin()) : std::nullopt;
}

KWinShellWindowRegistry::KWinShellWindowRegistry(
    ManagedWindowRegistry &registry,
    KWinShellVisibilityPublisher &visibility)
    : m_registry(registry)
    , m_visibility(visibility)
{
}

std::optional<ShellWindowGeneration>
KWinShellWindowRegistry::currentGeneration() const
{
    // AGENT-CONTRACT (ADR-0384): the fence is the last *published* visibility
    // generation, the same one task facts carry. Refreshing first keeps it
    // current; a rejected sample keeps the previous generation instead of
    // rejecting every dock action as stale.
    static_cast<void>(m_visibility.refreshForActionFence());
    if (m_visibility.revision() == 0) {
        return std::nullopt;
    }
    return ShellWindowGeneration{m_visibility.epoch(), m_visibility.revision()};
}

std::optional<ShellWindowTarget> KWinShellWindowRegistry::target(
    const QString &windowId) const
{
    if (!m_registry.window(windowId)) {
        return std::nullopt;
    }
    return ShellWindowTarget{windowId, m_registry.owner(windowId)};
}

KWinShellWindowActionExecutor::KWinShellWindowActionExecutor(
    ManagedWindowRegistry &registry,
    KWinHybridSession &hybridSession)
    : m_registry(registry)
    , m_hybridSession(hybridSession)
{
}

bool KWinShellWindowActionExecutor::execute(ShellWindowAction action,
                                            const ShellWindowTarget &target,
                                            QString *error)
{
    if (!target.hybridContainerId.isEmpty()) {
        // AGENT-GUARD: A shell request naming any Hybrid member enters the
        // existing group/page policy. Never fall through to a single KWin
        // member action, which would bypass collapsed task identity.
        return m_hybridSession.executeShellWindowAction(
            target.windowId, action, error);
    }
    auto *window = m_registry.window(target.windowId);
    auto *workspace = KWin::workspace();
    if (!window || !workspace || !m_registry.owner(target.windowId).isEmpty()) {
        if (error) {
            *error = QStringLiteral("the independent window changed before dispatch");
        }
        return false;
    }
    switch (action) {
    case ShellWindowAction::Activate:
        window->setMinimized(false);
        workspace->activateWindow(window, true);
        return true;
    case ShellWindowAction::Minimize:
        window->setMinimized(true);
        return true;
    case ShellWindowAction::Unminimize:
        window->setMinimized(false);
        return true;
    case ShellWindowAction::Close:
        window->closeWindow();
        return true;
    case ShellWindowAction::Raise:
        workspace->raiseWindow(window, true);
        return true;
    }
    return false;
}

KWinShellWindowActionsEndpoint::KWinShellWindowActionsEndpoint(
    ShellWindowActionController &actionController,
    ShellWindowIdentityController &identityController,
    KWinShellWindowIdentityPublisher &identityPublisher,
    ShellTaskFactsController &taskFactsController,
    KWinShellTaskFactsPublisher &taskFactsPublisher,
    KWinShellPanelOwnerSource &panelOwner,
    QDBusConnection connection,
    QObject *parent)
    : QObject(parent)
    , m_actionController(actionController)
    , m_identityController(identityController)
    , m_taskFactsController(taskFactsController)
    , m_connection(std::move(connection))
{
    connect(&identityPublisher, &KWinShellWindowIdentityPublisher::snapshotChanged,
            this, &KWinShellWindowActionsEndpoint::sendDirectedIdentityInvalidation);
    connect(&panelOwner, &KWinShellPanelOwnerSource::shellPanelOwnerChanged,
            this, &KWinShellWindowActionsEndpoint::sendDirectedIdentityInvalidation);
    connect(&taskFactsPublisher, &KWinShellTaskFactsPublisher::snapshotChanged,
            this, &KWinShellWindowActionsEndpoint::sendDirectedTaskFactsInvalidation);
    connect(&panelOwner, &KWinShellPanelOwnerSource::shellPanelOwnerChanged,
            this, &KWinShellWindowActionsEndpoint::sendDirectedTaskFactsInvalidation);
}

QByteArray KWinShellWindowActionsEndpoint::ActivateWindow(
    const QString &windowId, const QString &epoch, const QString &revision)
{
    return submit(ShellWindowAction::Activate, windowId, epoch, revision);
}

QByteArray KWinShellWindowActionsEndpoint::MinimizeWindow(
    const QString &windowId, const QString &epoch, const QString &revision)
{
    return submit(ShellWindowAction::Minimize, windowId, epoch, revision);
}

QByteArray KWinShellWindowActionsEndpoint::UnminimizeWindow(
    const QString &windowId, const QString &epoch, const QString &revision)
{
    return submit(ShellWindowAction::Unminimize, windowId, epoch, revision);
}

QByteArray KWinShellWindowActionsEndpoint::CloseWindow(
    const QString &windowId, const QString &epoch, const QString &revision)
{
    return submit(ShellWindowAction::Close, windowId, epoch, revision);
}

QByteArray KWinShellWindowActionsEndpoint::RaiseWindow(
    const QString &windowId, const QString &epoch, const QString &revision)
{
    return submit(ShellWindowAction::Raise, windowId, epoch, revision);
}

QByteArray KWinShellWindowActionsEndpoint::ActiveWindowIdentity()
{
    const QString caller = calledFromDBus() ? message().service() : QString{};
    auto reply = m_identityController.reply(caller);
    if (reply.authorized) {
        m_boundIdentityOwner = caller;
    }
    return std::move(reply.payload);
}

QByteArray KWinShellWindowActionsEndpoint::TaskListSnapshot()
{
    // ADR-0383: the controller says whether the caller authenticated; decoding
    // the reply again just to learn that cost a full JSON parse per read.
    const QString caller = calledFromDBus() ? message().service() : QString{};
    auto reply = m_taskFactsController.reply(caller);
    if (reply.authorized) {
        m_boundTaskFactsOwner = caller;
    }
    return std::move(reply.payload);
}

void KWinShellWindowActionsEndpoint::sendDirectedIdentityInvalidation()
{
    if (m_boundIdentityOwner.isEmpty() || !m_connection.isConnected()) {
        return;
    }
    // AGENT-GUARD: Exporting a normal Qt signal would broadcast focus timing
    // to every session-bus peer. Direct this no-payload hint only to the exact
    // shell owner that completed an authenticated identity read.
    QDBusMessage signal = QDBusMessage::createTargetedSignal(
        m_boundIdentityOwner, QString::fromLatin1(ShellObjectPath),
        QString::fromLatin1(ShellInterface),
        QString::fromLatin1(IdentityChangedSignal));
    m_connection.send(signal);
    if (!m_identityController.authorized(m_boundIdentityOwner)) {
        m_boundIdentityOwner.clear();
    }
}

void KWinShellWindowActionsEndpoint::sendDirectedTaskFactsInvalidation()
{
    if (m_boundTaskFactsOwner.isEmpty() || !m_connection.isConnected()) {
        return;
    }
    // AGENT-GUARD: Task titles, attention, workspace, and membership are
    // authenticated facts. A normal emitted Qt signal would broadcast their
    // timing even though the snapshot itself is protected.
    if (!m_taskFactsController.authorized(m_boundTaskFactsOwner)) {
        m_boundTaskFactsOwner.clear();
        return;
    }
    QDBusMessage signal = QDBusMessage::createTargetedSignal(
        m_boundTaskFactsOwner, QString::fromLatin1(ShellObjectPath),
        QString::fromLatin1(ShellInterface),
        QString::fromLatin1(TaskFactsChangedSignal));
    m_connection.send(signal);
}

QByteArray KWinShellWindowActionsEndpoint::submit(
    ShellWindowAction action,
    const QString &windowId,
    const QString &epoch,
    const QString &revision)
{
    const QString caller = calledFromDBus() ? message().service() : QString{};
    return encodeShellWindowActionResult(m_actionController.submit({
        caller, action, windowId, epoch, revision}));
}

} // namespace QindaQt::Compositor::KWinIntegration
