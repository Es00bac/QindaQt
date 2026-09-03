// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinshellwindowactions.h"

#include "kwinhybridsession.h"
#include "kwinshellwindowidentity.h"
#include "kwinshellvisibilitypublisher.h"
#include "managedwindowregistry.h"

#include <wayland/clientconnection.h>
#include <wayland/layershell_v1.h>
#include <wayland/surface.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
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

} // namespace

QtBusShellCredentialSource::QtBusShellCredentialSource(QDBusConnection connection)
    : m_connection(std::move(connection))
{
}

std::optional<qint64> QtBusShellCredentialSource::processIdForUniqueName(
    const QString &uniqueName) const
{
    if (!m_connection.isConnected() || uniqueName.isEmpty()
        || !uniqueName.startsWith(u':') || !m_connection.interface()) {
        return std::nullopt;
    }
    const QDBusReply<QVariantMap> reply =
        m_connection.interface()->serviceCredentials(uniqueName);
    if (!reply.isValid()) {
        return std::nullopt;
    }
    bool ok = false;
    const qulonglong processId = reply.value()
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
                this, &KWinShellPanelOwnerSource::shellPanelOwnerChanged);
    }
    connect(surface, &KWin::LayerSurfaceV1Interface::aboutToBeDestroyed,
            this, [this, surface] {
                m_surfaces.removeAll(surface);
                Q_EMIT shellPanelOwnerChanged();
            });
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
    if (!m_visibility.refreshForActionFence() || m_visibility.revision() == 0) {
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
    KWinShellPanelOwnerSource &panelOwner,
    QDBusConnection connection,
    QObject *parent)
    : QObject(parent)
    , m_actionController(actionController)
    , m_identityController(identityController)
    , m_connection(std::move(connection))
{
    connect(&identityPublisher, &KWinShellWindowIdentityPublisher::snapshotChanged,
            this, &KWinShellWindowActionsEndpoint::sendDirectedIdentityInvalidation);
    connect(&panelOwner, &KWinShellPanelOwnerSource::shellPanelOwnerChanged,
            this, &KWinShellWindowActionsEndpoint::sendDirectedIdentityInvalidation);
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
    const QByteArray result = m_identityController.snapshot(caller);
    const auto decoded = decodeShellWindowIdentitySnapshot(result);
    if (decoded && decoded->status != ShellWindowIdentityStatus::Unauthorized) {
        m_boundIdentityOwner = caller;
    }
    return result;
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
