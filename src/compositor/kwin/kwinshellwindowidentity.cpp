// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinshellwindowidentity.h"

#include "kwinshellvisibilitypublisher.h"
#include "shellvisibilitywindowadmission.h"

#include <config-kwin.h>
#include <window.h>
#include <workspace.h>
#if KWIN_BUILD_X11
#include <x11window.h>
#endif

#include <QTimer>
#include <QUuid>

#include <limits>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

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

} // namespace

KWinShellWindowIdentityPublisher::KWinShellWindowIdentityPublisher(
    KWinShellVisibilityPublisher &visibility, QObject *parent)
    : QObject(parent)
    , m_visibility(visibility)
    , m_store(visibility.epoch())
{
    auto *const compositorWorkspace = KWin::workspace();
    Q_ASSERT(compositorWorkspace);
    connect(&m_visibility, &KWinShellVisibilityPublisher::snapshotChanged,
            this, &KWinShellWindowIdentityPublisher::scheduleRefresh);
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
    connect(compositorWorkspace, &KWin::Workspace::windowActivated,
            this, [this](KWin::Window *) { scheduleRefresh(); });
    for (auto *window : compositorWorkspace->windows()) {
        trackWindow(window);
    }
    refresh();
}

KWinShellWindowIdentityPublisher::~KWinShellWindowIdentityPublisher() = default;

const QByteArray &KWinShellWindowIdentityPublisher::snapshotJson()
{
    m_refreshScheduled = false;
    refresh();
    return m_store.snapshotJson();
}

void KWinShellWindowIdentityPublisher::trackWindow(KWin::Window *window)
{
    if (!window || m_connections.contains(window)) {
        return;
    }
    QVector<QMetaObject::Connection> connections;
    connections.append(connect(window, &KWin::Window::applicationMenuChanged,
                               this, &KWinShellWindowIdentityPublisher::scheduleRefresh));
    connections.append(connect(window, &QObject::destroyed, this,
                               [this, window] {
                                   m_connections.remove(window);
                                   scheduleRefresh();
                               }));
    m_connections.insert(window, std::move(connections));
}

void KWinShellWindowIdentityPublisher::forgetWindow(KWin::Window *window)
{
    const auto connections = m_connections.take(window);
    for (const auto &connection : connections) {
        disconnect(connection);
    }
}

void KWinShellWindowIdentityPublisher::scheduleRefresh()
{
    if (m_refreshScheduled) {
        return;
    }
    m_refreshScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshScheduled = false;
        refresh();
    });
}

void KWinShellWindowIdentityPublisher::refresh()
{
    QString error;
    const auto candidate = sample(&error);
    if (!candidate) {
        if (m_store.markUnavailable(QStringLiteral("identity-sampling-failed"),
                                    error)) {
            Q_EMIT snapshotChanged();
        }
        return;
    }
    const auto result = m_store.publish(*candidate, &error);
    if (result == ShellWindowIdentityPublishResult::Published) {
        Q_EMIT snapshotChanged();
    } else if (result == ShellWindowIdentityPublishResult::Rejected
               || result == ShellWindowIdentityPublishResult::RevisionExhausted) {
        if (m_store.markUnavailable(QStringLiteral("identity-invalid"), error)) {
            Q_EMIT snapshotChanged();
        }
    }
}

std::optional<ShellWindowIdentityCandidate>
KWinShellWindowIdentityPublisher::sample(QString *error)
{
    if (!m_visibility.refreshForActionFence() || m_visibility.revision() == 0) {
        if (error) *error = QStringLiteral("the window-action generation is unavailable");
        return std::nullopt;
    }
    ShellWindowIdentityCandidate candidate{
        {m_visibility.epoch(), m_visibility.revision()}, std::nullopt};
    auto *const active = KWin::workspace() ? KWin::workspace()->activeWindow() : nullptr;
    if (!admitsShellVisibilityWindow(admission(active))) {
        if (error) error->clear();
        return candidate;
    }
    const QUuid uuid = active->internalId();
    if (uuid.isNull()) {
        if (error) *error = QStringLiteral("the active KWin window has no UUID");
        return std::nullopt;
    }
    ShellWindowIdentityFacts facts;
    facts.windowId = uuid.toString(QUuid::WithoutBraces);
    // AGENT-CONTRACT: KWin supplies this from wl_client credentials for native
    // Wayland and from XRes LOCAL_CLIENT_PID for the exact X client on X11.
    // Never replace it with a client-supplied PID property.
    const pid_t pid = active->pid();
    if (pid > 1
        && static_cast<quint64>(pid)
            <= static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        facts.processId = static_cast<qint64>(pid);
    }
#if KWIN_BUILD_X11
    if (const auto *x11 = qobject_cast<const KWin::X11Window *>(active);
        x11 && x11->window() != XCB_WINDOW_NONE) {
        facts.appMenuWindowId = static_cast<quint32>(x11->window());
    }
#endif
    const QString serviceName = active->applicationMenuServiceName();
    const QString objectPath = active->applicationMenuObjectPath();
    if (!serviceName.isEmpty() && !objectPath.isEmpty()) {
        facts.appMenuServiceName = serviceName;
        facts.appMenuObjectPath = objectPath;
    }
    candidate.activeWindow = std::move(facts);
    if (error) error->clear();
    return candidate;
}

} // namespace QindaQt::Compositor::KWinIntegration
