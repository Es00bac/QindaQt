// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinshelltaskfacts.h"
#include "qindaqt/compositor/foreignwindowidentity.h"
#include "qindaqt/compositor/wineidentitycache.h"

#include "coalescedchangenotifier.h"
#include "foreignidentityresolver.h"
#include "hybridtaskidentitypolicy.h"
#include "kwinhybridsession.h"
#include "kwinoutputinventory.h"
#include "kwinshellvisibilitypublisher.h"
#include "kwinshellwindowactions.h"
#include "managedwindowregistry.h"
#include "qindaqt/compositor/containercontrolbridge.h"

#include "windowcontainer.h"

#include <core/output.h>
#include <virtualdesktops.h>
#include <window.h>
#include <workspace.h>

#include <QDir>
#include <QHash>
#include <QSet>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

struct ContainerProjection final
{
    ShellTaskContainer lineage;
    TaskContainerIdentity identity;
};

std::optional<ContainerProjection> bridgeProjection(
    ContainerControlBridge &bridge, const QString &containerId, QString *error)
{
    const auto revision = bridge.revision(containerId);
    const auto snapshot = bridge.snapshot(containerId);
    if (!revision || !snapshot) {
        setError(error, QStringLiteral("control-bridge container lineage is unavailable"));
        return std::nullopt;
    }
    QString parseError;
    const auto container = Core::WindowContainer::fromJson(*snapshot, &parseError);
    if (!container) {
        setError(error, parseError);
        return std::nullopt;
    }
    auto identity = HybridTaskIdentityPolicy::planContainer(*container, {}, error);
    if (!identity) {
        return std::nullopt;
    }
    return ContainerProjection{
        {containerId, *revision, ShellTaskContainerAuthority::ControlBridge},
        std::move(*identity)};
}

std::optional<ContainerProjection> hybridProjection(
    KWinHybridSession &hybrid, const QString &containerId, QString *error)
{
    const quint64 revision = hybrid.topologyRevision();
    for (const auto &identity : hybrid.taskIdentityPlans()) {
        if (identity.containerId == containerId && revision > 0) {
            return ContainerProjection{
                {containerId, revision, ShellTaskContainerAuthority::HybridProcess},
                identity};
        }
    }
    setError(error, QStringLiteral("hybrid container task identity is unavailable"));
    return std::nullopt;
}

} // namespace

KWinShellTaskFactsPublisher::KWinShellTaskFactsPublisher(
    ManagedWindowRegistry &registry, KWinOutputInventory &outputs,
    KWinShellVisibilityPublisher &visibility, ContainerControlBridge &bridge,
    KWinHybridSession &hybrid, KWinShellPanelOwnerSource &panelOwner,
    QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_outputs(outputs)
    , m_visibility(visibility)
    , m_bridge(bridge)
    , m_hybrid(hybrid)
    , m_panelOwner(panelOwner)
    , m_store(visibility.epoch())
    , m_foreignIdentity(std::make_unique<ForeignIdentityResolver>(
          std::make_shared<Compositor::WineIdentityCache>(
              Compositor::WineIdentityCache::defaultCacheRoot(), QDir::homePath())))
    , m_refresh(std::make_unique<CoalescedChangeNotifier>(
          RefreshIntervalMilliseconds, [this](const QStringList &) { refresh(); }))
{
    auto *const compositorWorkspace = KWin::workspace();
    Q_ASSERT(compositorWorkspace);
    connect(&m_registry, &ManagedWindowRegistry::windowsChanged,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    // Titles are task facts; they arrive already coalesced per window.
    connect(&m_registry, &ManagedWindowRegistry::windowCaptionsChanged,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    connect(m_foreignIdentity.get(), &ForeignIdentityResolver::identityChanged,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    connect(&m_outputs, &KWinOutputInventory::inventoryChanged,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    connect(&m_visibility, &KWinShellVisibilityPublisher::snapshotChanged,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    connect(&m_bridge, &ContainerControlBridge::containerCommitted,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    connect(&m_hybrid, &KWinHybridSession::shellVisibilityStateChanged,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    connect(&m_panelOwner, &KWinShellPanelOwnerSource::shellPanelOwnerChanged,
            this, &KWinShellTaskFactsPublisher::scheduleRefresh);
    connect(compositorWorkspace, &KWin::Workspace::windowAdded,
            this, [this](KWin::Window *window) {
                trackWindow(window);
                scheduleRefresh();
            });
    connect(compositorWorkspace, &KWin::Workspace::windowRemoved,
            this, [this](KWin::Window *window) {
                forgetWindow(window);
                scheduleRefresh();
            });
    if (auto *desktops = KWin::VirtualDesktopManager::self()) {
        connect(desktops, &KWin::VirtualDesktopManager::desktopAdded,
                this, [this](KWin::VirtualDesktop *) { scheduleRefresh(); });
        connect(desktops, &KWin::VirtualDesktopManager::desktopRemoved,
                this, [this](KWin::VirtualDesktop *) { scheduleRefresh(); });
        connect(desktops, &KWin::VirtualDesktopManager::desktopMoved,
                this, [this](KWin::VirtualDesktop *, int) { scheduleRefresh(); });
    }
    for (auto *window : compositorWorkspace->windows()) {
        trackWindow(window);
    }
    refresh();
}

KWinShellTaskFactsPublisher::~KWinShellTaskFactsPublisher() = default;

const QByteArray &KWinShellTaskFactsPublisher::snapshotJson()
{
    // AGENT-CONTRACT (ADR-0383): serve the cached, already encoded generation.
    // Every change that can alter it schedules a refresh, which publishes and
    // then invalidates the reader; re-sampling here doubled the work of every
    // change and let a reader defeat the rate limit.
    return m_store.snapshotJson();
}

void KWinShellTaskFactsPublisher::trackWindow(KWin::Window *window)
{
    if (!window || m_connections.contains(window)) {
        return;
    }
    const auto changed = [this] { scheduleRefresh(); };
    QVector<QMetaObject::Connection> connections;
    connections.append(connect(window, &KWin::Window::outputChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::desktopsChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::activeChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::minimizedChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::hiddenChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::maximizedChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::demandsAttentionChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::fullScreenChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::desktopFileNameChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::windowClassChanged,
                               this, changed));
    connections.append(connect(window, &QObject::destroyed, this,
                               [this, window] { m_connections.remove(window); }));
    m_connections.insert(window, std::move(connections));
}

void KWinShellTaskFactsPublisher::forgetWindow(KWin::Window *window)
{
    if (window) {
        m_foreignIdentity->forget(m_registry.windowId(window));
    }
    const auto connections = m_connections.take(window);
    for (const auto &connection : connections) {
        disconnect(connection);
    }
}

void KWinShellTaskFactsPublisher::scheduleRefresh()
{
    m_refresh->notify();
}

void KWinShellTaskFactsPublisher::refresh()
{
    ++m_samples;
    QString error;
    const auto candidate = sample(&error);
    if (!candidate) {
        if (m_store.markUnavailable(QStringLiteral("task-facts-sampling-failed"),
                                    error)) {
            Q_EMIT snapshotChanged();
        }
        return;
    }
    const auto result = m_store.publish(*candidate, &error);
    if (result == ShellTaskFactsPublishResult::Published) {
        Q_EMIT snapshotChanged();
    } else if (result == ShellTaskFactsPublishResult::Rejected
               || result == ShellTaskFactsPublishResult::RevisionExhausted) {
        if (m_store.markUnavailable(QStringLiteral("task-facts-invalid"), error)) {
            Q_EMIT snapshotChanged();
        }
    }
}

std::optional<ShellTaskFactsCandidate>
KWinShellTaskFactsPublisher::sample(QString *error)
{
    // AGENT-CONTRACT (ADR-0384): task facts carry the visibility generation as
    // their action fence but never depend on the latest visibility sample
    // being valid. refreshForActionFence() closes the invalidation window so
    // the fence is current; a rejected visibility batch keeps its last
    // published generation, and the dock keeps working on it.
    static_cast<void>(m_visibility.refreshForActionFence());
    if (!m_outputs.available() || m_visibility.revision() == 0) {
        setError(error, QStringLiteral("task facts dependencies are unavailable"));
        return std::nullopt;
    }
    auto *const desktops = KWin::VirtualDesktopManager::self();
    if (!desktops || desktops->desktops().isEmpty()) {
        setError(error, QStringLiteral("task workspace inventory is unavailable"));
        return std::nullopt;
    }
    ShellTaskFactsCandidate candidate;
    candidate.actionGeneration = {m_visibility.epoch(), m_visibility.revision()};
    for (const auto &output : m_outputs.entries()) {
        candidate.outputs.append({output.name});
    }
    for (const auto *desktop : desktops->desktops()) {
        if (!desktop) {
            setError(error, QStringLiteral("task workspace inventory contains null"));
            return std::nullopt;
        }
        candidate.workspaces.append({desktop->id()});
    }

    QHash<QString, ShellTaskWindowRole> roles;
    const std::optional<qint64> shellProcessId =
        m_panelOwner.shellPanelProcessId();
    for (const QString &containerId : m_registry.containerIds()) {
        auto projection = m_bridge.contains(containerId)
            ? bridgeProjection(m_bridge, containerId, error)
            : hybridProjection(m_hybrid, containerId, error);
        if (!projection) {
            return std::nullopt;
        }
        candidate.containers.append(projection->lineage);
        for (const auto &member : projection->identity.members) {
            roles.insert(member.windowId,
                         member.primary ? ShellTaskWindowRole::ContainerPrimary
                                        : ShellTaskWindowRole::ContainerMember);
        }
    }

    for (const QString &windowId : m_registry.windowIds()) {
        auto *window = m_registry.window(windowId);
        // A window can be between outputs for a moment after a hot-unplug;
        // report the output it lies on rather than void every task.
        auto *const output = window
            ? (window->output() ? window->output()
                                : KWin::workspace()->outputAt(
                                      window->frameGeometry().center()))
            : nullptr;
        if (!window || !output) {
            setError(error, QStringLiteral("managed task window is unavailable"));
            return std::nullopt;
        }
        const QString containerId = m_registry.owner(windowId);
        if (!containerId.isEmpty() && !roles.contains(windowId)) {
            setError(error, QStringLiteral("grouped task window has no atomic role"));
            return std::nullopt;
        }
        // ADR-0169: a window whose class is an opaque launcher key
        // (`steam_app_<n>`, a Wine shim, or nothing at all) is reported as the
        // program actually behind it, read from the client's own command line
        // through KWin's authenticated PID. Every other class is left exactly
        // as reported. This stays raw identity; naming and icon policy remain
        // in the shell.
        const QString resourceClass = window->resourceClass();
        QString clientExecutable;
        QString steamName;
        if (Compositor::isOpaqueLauncherClass(resourceClass)) {
            // ADR-0230 layered on ADR-0169: a real `steam_app_<n>` title takes
            // its human name from Steam's appmanifest, and the executable's PE
            // icon is refreshed in the shared cache the shell's confined icon
            // path reads. AGENT-GUARD (ADR-0384): never read /proc or the
            // filesystem here; this runs for every window on every sample.
            // The resolver reads each window's command line once and does the
            // Steam/icon work on its worker.
            const auto identity = m_foreignIdentity->identity(
                windowId, static_cast<qint64>(window->pid()), resourceClass);
            clientExecutable = identity.clientExecutable;
            steamName = identity.steamName;
        }
        const QString applicationId = Compositor::resolveApplicationId(
            window->desktopFileName(), resourceClass, clientExecutable);
        const QString applicationName = Compositor::resolveApplicationName(
            resourceClass, clientExecutable, applicationId, steamName);
        const bool groupedMaximized = !containerId.isEmpty()
            && m_hybrid.isContainerMaximized(containerId);
        // AGENT-CONTRACT: A container rename (ContainerAppearance::name) is a
        // presentation override of the collapsed identity's reported title,
        // not a rewrite of any member's real KWin caption. Only the primary
        // representative's fact feeds the dock's one entry per container
        // (see docs/wiki/architecture/window-containers.md); applying it to
        // every member's fact here is harmless because non-primary facts are
        // suppressed from the task list regardless.
        const QString containerName = containerId.isEmpty()
            ? QString{} : m_hybrid.containerAppearance(containerId).name;
        const bool isContainerPrimary = !containerId.isEmpty()
            && roles.value(windowId) == ShellTaskWindowRole::ContainerPrimary;
        ShellTaskWindow facts{
            .windowId = windowId,
            .applicationId = applicationId,
            .applicationName = applicationName,
            .title = containerName.isEmpty() ? window->caption() : containerName,
            // AGENT-CONTRACT: colorHex is a ContainerAppearance override and
            // must be empty for every non-primary fact; the wire codec
            // rejects a non-empty value on any other role
            // (validateShellTaskFactsCandidate), matching the invariant that
            // only the primary feeds the dock's one collapsed entry.
            .colorHex = isContainerPrimary
                ? m_hybrid.containerAppearance(containerId).colorHex : QString{},
            .role = containerId.isEmpty() ? ShellTaskWindowRole::Standalone
                                          : roles.value(windowId),
            .type = window->isNormalWindow() ? ShellTaskWindowType::Normal
                                             : ShellTaskWindowType::NonNormal,
            .owner = shellProcessId
                    && static_cast<qint64>(window->pid()) == *shellProcessId
                ? ShellTaskWindowOwner::BoundShell
                : ShellTaskWindowOwner::Application,
            .active = window->isActive(),
            .minimized = window->isMinimized(),
            // ADR-0203: rolled up to its icon chip (independent windows only);
            // the task list renders a rolled hint, never a second entry.
            .iconified = m_hybrid.isWindowIconified(windowId),
            .maximized = window->maximizeMode() == KWin::MaximizeFull
                || groupedMaximized,
            .fullscreen = window->isFullScreen(),
            .demandsAttention = window->isDemandingAttention(),
            .outputId = output->name(),
            .workspaceIds = window->isOnAllDesktops()
                ? QStringList{} : window->desktopIds(),
            .onAllWorkspaces = window->isOnAllDesktops(),
            .containerId = containerId,
        };
        candidate.windows.append(std::move(facts));
    }
    if (!validateShellTaskFactsCandidate(candidate, error)) {
        return std::nullopt;
    }
    setError(error, {});
    return candidate;
}

} // namespace QindaQt::Compositor::KWinIntegration
