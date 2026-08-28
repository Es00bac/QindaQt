// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinshellvisibilitypublisher.h"

#include "kwinoutputinventory.h"
#include "shellvisibilityrefreshscheduler.h"
#include "shellvisibilitywindowadmission.h"
#include "managedwindowregistry.h"

#include <activities.h>
#include <config-kwin.h>
#include <effect/globals.h>
#include <virtualdesktops.h>
#include <window.h>
#include <workspace.h>

#include <QRectF>
#include <QTimer>
#include <QUuid>

#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto NoActivitiesId = "00000000-0000-0000-0000-000000000000";

std::optional<QRect> alignedLogicalGeometry(const QRectF &geometry)
{
    const long double left = std::floor(static_cast<long double>(geometry.x()));
    const long double top = std::floor(static_cast<long double>(geometry.y()));
    const long double right =
        std::ceil(static_cast<long double>(geometry.x()) + geometry.width());
    const long double bottom =
        std::ceil(static_cast<long double>(geometry.y()) + geometry.height());
    const auto minimum = static_cast<long double>(std::numeric_limits<int>::min());
    const auto maximum = static_cast<long double>(std::numeric_limits<int>::max());
    if (!geometry.isValid() || !std::isfinite(geometry.x())
        || !std::isfinite(geometry.y()) || !std::isfinite(geometry.width())
        || !std::isfinite(geometry.height()) || left < minimum || top < minimum
        || right > maximum || bottom > maximum || right <= left || bottom <= top
        || right - left > maximum || bottom - top > maximum) {
        return std::nullopt;
    }
    return QRect(static_cast<int>(left), static_cast<int>(top),
                 static_cast<int>(right - left), static_cast<int>(bottom - top));
}

ShellVisibilityWindowAdmission admission(const KWin::Window *window)
{
    return {
        .exists = window != nullptr,
        .deleted = window && window->isDeleted(),
        .internal = window && window->isInternal(),
        .managed = window && window->isClient(),
        .desktop = window && window->isDesktop(),
        .dock = window && window->isDock(),
        .splash = window && window->isSplash(),
        .tooltip = window && window->isTooltip(),
        .menu = window && (window->isMenu() || window->isDropdownMenu()
                          || window->isPopupMenu() || window->isComboBox()),
        .popup = window && window->isPopupWindow(),
        .normal = window && window->isNormalWindow(),
        .dialog = window && window->isDialog(),
        .utility = window && window->isUtility(),
        .transient = window && window->isTransient(),
    };
}

bool isVisibleUserWindow(const KWin::Window *window)
{
    return admitsShellVisibilityWindow(admission(window));
}

} // namespace

KWinShellVisibilityPublisher::KWinShellVisibilityPublisher(
    ManagedWindowRegistry &registry,
    KWinOutputInventory &outputInventory,
    QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_outputInventory(outputInventory)
    , m_store(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_scheduler(std::make_unique<ShellVisibilityRefreshScheduler>(
          [this] { refresh(); }, this))
{
    auto *const compositorWorkspace = KWin::workspace();
    Q_ASSERT(compositorWorkspace);
    connect(&outputInventory, &KWinOutputInventory::inventoryChanged,
            this, &KWinShellVisibilityPublisher::scheduleRefresh);
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
    connect(compositorWorkspace, &KWin::Workspace::currentDesktopChanged,
            this, [this](KWin::VirtualDesktop *, KWin::Window *) {
                scheduleRefresh();
            });
    connect(compositorWorkspace, &KWin::Workspace::currentActivityChanged,
            this, &KWinShellVisibilityPublisher::scheduleRefresh);

    if (auto *desktopManager = KWin::VirtualDesktopManager::self()) {
        connect(desktopManager, &KWin::VirtualDesktopManager::desktopAdded,
                this, [this](KWin::VirtualDesktop *) { scheduleRefresh(); });
        connect(desktopManager, &KWin::VirtualDesktopManager::desktopRemoved,
                this, [this](KWin::VirtualDesktop *) { scheduleRefresh(); });
    }
#if KWIN_BUILD_ACTIVITIES
    if (auto *activities = compositorWorkspace->activities()) {
        connect(activities, &KWin::Activities::added,
                this, [this](const QString &) { scheduleRefresh(); });
        connect(activities, &KWin::Activities::removed,
                this, [this](const QString &) { scheduleRefresh(); });
    }
#endif

    for (auto *window : compositorWorkspace->windows()) {
        trackWindow(window);
    }
    // The D-Bus object is registered only after construction, so this first
    // synchronous generation cannot race a client and needs no change signal.
    refresh();
}

KWinShellVisibilityPublisher::~KWinShellVisibilityPublisher() = default;

const QByteArray &KWinShellVisibilityPublisher::snapshotJson() const noexcept
{
    return m_store.snapshotJson();
}

void KWinShellVisibilityPublisher::setHybridMaximizedProvider(
    HybridMaximizedProvider provider)
{
    m_hybridMaximized = std::move(provider);
    scheduleRefresh();
}

void KWinShellVisibilityPublisher::invalidate()
{
    scheduleRefresh();
}

void KWinShellVisibilityPublisher::trackWindow(KWin::Window *window)
{
    if (!isVisibleUserWindow(window) || m_windowConnections.contains(window)) {
        return;
    }
    QVector<QMetaObject::Connection> connections;
    const auto changed = [this] { scheduleRefresh(); };
    connections.append(connect(window, &KWin::Window::outputChanged, this, changed));
    connections.append(connect(window, &KWin::Window::frameGeometryChanged,
                               this, [changed](const KWin::RectF &) { changed(); }));
    connections.append(connect(window, &KWin::Window::maximizedChanged, this, changed));
    connections.append(connect(window, &KWin::Window::minimizedChanged, this, changed));
    connections.append(connect(window, &KWin::Window::hiddenChanged, this, changed));
    connections.append(connect(window, &KWin::Window::hiddenByShowDesktopChanged,
                               this, changed));
    connections.append(connect(window, &KWin::Window::activeChanged, this, changed));
    connections.append(connect(window, &KWin::Window::desktopsChanged, this, changed));
    connections.append(connect(window, &KWin::Window::activitiesChanged, this, changed));
    connections.append(connect(window, &QObject::destroyed, this,
                               [this, window] {
                                   m_windowConnections.remove(window);
                                   scheduleRefresh();
                               }));
    m_windowConnections.insert(window, std::move(connections));
}

void KWinShellVisibilityPublisher::forgetWindow(KWin::Window *window)
{
    const auto connections = m_windowConnections.take(window);
    for (const auto &connection : connections) {
        disconnect(connection);
    }
}

void KWinShellVisibilityPublisher::scheduleRefresh()
{
    m_scheduler->request();
}

void KWinShellVisibilityPublisher::refresh()
{
    QString error;
    const auto candidate = sample(&error);
    if (!candidate) {
        handleFailure(QStringLiteral("snapshot-sampling-failed"), error);
        return;
    }
    const auto result = m_store.publish(*candidate, &error);
    if (result == ShellVisibilityPublishResult::Published) {
        m_outage = false;
        Q_EMIT snapshotChanged();
    } else if (result == ShellVisibilityPublishResult::Unchanged) {
        m_outage = false;
    } else if (result == ShellVisibilityPublishResult::RevisionExhausted) {
        handleFailure(QStringLiteral("revision-exhausted"), error);
    } else {
        handleFailure(QStringLiteral("snapshot-invalid"), error);
    }
}

void KWinShellVisibilityPublisher::handleFailure(
    const QString &code,
    const QString &message)
{
    // AGENT-GUARD: Never leave a stale status:ok generation externally current.
    // Shell fallback is safe-visible/reserved only after it sees unavailable.
    if (m_store.markUnavailable(code, message)) {
        Q_EMIT snapshotChanged();
    }
    qWarning("QindaQt shell visibility snapshot is unavailable: %s",
             qPrintable(message));
    if (m_outage) {
        return;
    }
    m_outage = true;
    // One retry absorbs transient KWin change sequences. Persistent failure
    // remains explicitly unavailable until any later source signal retries.
    QTimer::singleShot(50, this, [this] { scheduleRefresh(); });
}

std::optional<ShellVisibilitySnapshotCandidate>
KWinShellVisibilityPublisher::sample(QString *error) const
{
    ShellVisibilitySnapshotCandidate candidate;
    auto *const compositorWorkspace = KWin::workspace();
    auto *const desktopManager = KWin::VirtualDesktopManager::self();
    if (!compositorWorkspace || !desktopManager || !desktopManager->currentDesktop()) {
        if (error) {
            *error = QStringLiteral("KWin desktop scope is unavailable");
        }
        return std::nullopt;
    }
    candidate.scope.workspaceId = desktopManager->currentDesktop()->id();
#if KWIN_BUILD_ACTIVITIES
    if (auto *activities = compositorWorkspace->activities();
        activities && !activities->current().isEmpty()) {
        candidate.scope.activityId = activities->current();
    }
#endif
    if (candidate.scope.activityId.isEmpty()) {
        candidate.scope.activityId = QString::fromLatin1(NoActivitiesId);
    }

    if (!m_outputInventory.available() || m_outputInventory.generation() == 0) {
        if (error) {
            *error = QStringLiteral("the shared output inventory is unavailable");
        }
        return std::nullopt;
    }
    candidate.outputGeneration = m_outputInventory.generation();
    for (const auto &output : m_outputInventory.entries()) {
        // AGENT-CONTRACT: Visibility derives from the immutable generation
        // returned by Outputs(), never a second live Workspace sample.
        candidate.outputs.append(
            {output.name, output.visibilityGeometry, output.scale});
    }

    for (auto *window : compositorWorkspace->windows()) {
        if (!isVisibleUserWindow(window)) {
            continue;
        }
        const auto geometry = alignedLogicalGeometry(window->frameGeometry());
        const auto uuid = window->internalId();
        if (!window->output() || !geometry || uuid.isNull()) {
            if (error) {
                *error = QStringLiteral("an admitted KWin window is unrepresentable");
            }
            return std::nullopt;
        }
        const auto owner = m_registry.owner(
            uuid.toString(QUuid::WithoutBraces));
        const bool hybridMaximized = !owner.isEmpty() && m_hybridMaximized
            && m_hybridMaximized(owner);
        candidate.windows.append({
            .id = uuid.toString(QUuid::WithoutBraces),
            .outputId = window->output()->name(),
            .frameGeometry = *geometry,
            .workspaceIds = window->desktopIds(),
            .activityIds = window->activities(),
            .onAllWorkspaces = window->isOnAllDesktops(),
            .active = window->isActive(),
            .maximized = window->maximizeMode() == KWin::MaximizeFull
                || hybridMaximized,
            .minimized = window->isMinimized(),
            .hidden = window->isHidden() || window->isHiddenByShowDesktop(),
        });
    }
    return candidate;
}

} // namespace QindaQt::Compositor::KWinIntegration
