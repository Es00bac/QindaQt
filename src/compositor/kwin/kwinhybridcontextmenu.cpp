// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "kwingroupcontextmenu.h"
#include "hybridinteractionruntime.h"
#include "kwintaskidentitymanager.h"
#include "managedwindowregistry.h"

#include <activities.h>
#include <config-kwin.h>
#include <core/output.h>
#include <virtualdesktops.h>
#include <window.h>
#include <workspace.h>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

QString workspaceLabel(const KWin::VirtualDesktop &desktop)
{
    return desktop.name().isEmpty()
        ? QStringLiteral("Workspace %1").arg(desktop.x11DesktopNumber())
        : desktop.name();
}

QString outputLabel(const KWin::LogicalOutput &output)
{
    return output.description().isEmpty() ? output.name()
                                          : output.description();
}

std::optional<GroupContextMenuState> contextState(
    const KWin::Window &window,
    QString *error)
{
    auto *const workspace = KWin::workspace();
    auto *const desktopManager = KWin::VirtualDesktopManager::self();
    if (!workspace || !desktopManager) {
        fail(error, QStringLiteral("KWin context inventory is unavailable"));
        return std::nullopt;
    }

    GroupContextMenuState state{
        .keepAbove = window.keepAbove(),
        .keepBelow = window.keepBelow(),
        .pinnedToAllWorkspaces = window.isOnAllDesktops(),
        .onAllActivities = window.isOnAllActivities(),
        .workspaces = {},
        .activities = {},
        .outputs = {},
    };
    for (auto *desktop : desktopManager->desktops()) {
        if (!desktop) {
            continue;
        }
        state.workspaces.append({
            desktop->id(),
            workspaceLabel(*desktop),
            !state.pinnedToAllWorkspaces && window.isOnDesktop(desktop),
        });
    }
#if KWIN_BUILD_ACTIVITIES
    if (auto *activities = workspace->activities()) {
        for (const auto &activityId : activities->all()) {
            // KWin's public Activities adapter exposes stable IDs but not the
            // optional display-name cache. The ID remains an unambiguous label
            // when that service metadata is unavailable.
            state.activities.append({
                activityId,
                activityId,
                !state.onAllActivities && window.isOnActivity(activityId),
            });
        }
    }
#endif
    for (auto *output : workspace->outputs()) {
        if (!output) {
            continue;
        }
        state.outputs.append({
            output->name(), outputLabel(*output), window.output() == output});
    }
    return state;
}

bool applyContextCommand(
    KWin::Window &window,
    const GroupContextMenuCommand &command,
    QString *error)
{
    auto *const workspace = KWin::workspace();
    auto *const desktopManager = KWin::VirtualDesktopManager::self();
    if (!workspace || !desktopManager) {
        return fail(error, QStringLiteral("KWin context inventory is unavailable"));
    }

    switch (command.kind) {
    case GroupContextMenuCommandKind::SetKeepAbove:
        window.setKeepAbove(command.enabled);
        return true;
    case GroupContextMenuCommandKind::SetKeepBelow:
        window.setKeepBelow(command.enabled);
        return true;
    case GroupContextMenuCommandKind::SetPinnedToAllWorkspaces:
        window.setOnAllDesktops(command.enabled);
        return true;
    case GroupContextMenuCommandKind::ToggleWorkspace: {
        auto *const desktop = desktopManager->desktopForId(command.destinationId);
        if (!desktop) {
            return fail(error, QStringLiteral("selected workspace no longer exists"));
        }
        if (window.isOnAllDesktops() && command.enabled) {
            window.setDesktops({desktop});
        } else if (command.enabled) {
            window.enterDesktop(desktop);
        } else {
            window.leaveDesktop(desktop);
        }
        return true;
    }
    case GroupContextMenuCommandKind::SetAllActivities:
#if KWIN_BUILD_ACTIVITIES
        if (command.enabled) {
            window.setOnAllActivities(true);
            return true;
        }
        if (auto *activities = workspace->activities();
            activities && !activities->current().isEmpty()) {
            window.setOnActivities({activities->current()});
            return true;
        }
#endif
        return fail(error, QStringLiteral("current activity is unavailable"));
    case GroupContextMenuCommandKind::ToggleActivity:
#if KWIN_BUILD_ACTIVITIES
        if (auto *activities = workspace->activities();
            activities && activities->all().contains(command.destinationId)) {
            if (window.isOnAllActivities() && command.enabled) {
                window.setOnActivities({command.destinationId});
            } else {
                window.setOnActivity(command.destinationId, command.enabled);
            }
            return true;
        }
#endif
        return fail(error, QStringLiteral("selected activity no longer exists"));
    case GroupContextMenuCommandKind::MoveToOutput:
        if (auto *output = workspace->findOutput(command.destinationId)) {
            window.sendToOutput(output);
            return true;
        }
        return fail(error, QStringLiteral("selected output no longer exists"));
    }
    return fail(error, QStringLiteral("unknown group context command"));
}

} // namespace

void KWinHybridSession::initializeGroupContextMenu()
{
    m_groupContextMenu = std::make_unique<KWinGroupContextMenu>(
        [this](const QString &containerId, QString *error)
            -> std::optional<GroupContextMenuState> {
            const QString representativeId = m_taskIdentity
                ? m_taskIdentity->primaryWindowId(containerId) : QString{};
            auto *const representative = m_registry.window(representativeId);
            if (!ready() || representativeId.isEmpty() || !representative
                || m_registry.owner(representativeId) != containerId) {
                fail(error, QStringLiteral("group context representative is stale"));
                return std::nullopt;
            }
            return contextState(*representative, error);
        },
        [this](const QString &containerId,
               const GroupContextMenuCommand &command,
               QString *error) {
            const QString representativeId = m_taskIdentity
                ? m_taskIdentity->primaryWindowId(containerId) : QString{};
            auto *const representative = m_registry.window(representativeId);
            if (!ready() || representativeId.isEmpty() || !representative
                || m_registry.owner(representativeId) != containerId) {
                return fail(error,
                            QStringLiteral("group context representative is stale"));
            }
            // AGENT-CONTRACT: Mutate exactly one active representative. The
            // queued KWinGroupContextManager adopts its final output,
            // workspace, activity, and layer as one scene transaction.
            return applyContextCommand(*representative, command, error);
        });
}

void KWinHybridSession::showGroupContextMenu(
    const QString &containerId,
    const QPointF &globalPosition)
{
    if (!ready() || !m_groupContextMenu) {
        return;
    }
    QString error;
    if (!m_groupContextMenu->popupForContainer(
            containerId, globalPosition, &error)) {
        qWarning("QindaQt could not open group context menu: %s",
                 qPrintable(error));
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
