// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "kwingroupcontextmenu.h"
#include "hybridinteractionruntime.h"
#include "hybridcontainerplacement.h"
#include "kwininteractionfilter.h"
#include "kwinchromemanager.h"
#include "kwintaskidentitymanager.h"
#include "managedwindowregistry.h"
#include "memberchromevisibilitycontroller.h"

#include <activities.h>
#include <config-kwin.h>
#include <core/output.h>
#include <virtualdesktops.h>
#include <window.h>
#include <workspace.h>

#include <QInputDialog>
#include <QLineEdit>

#include <algorithm>
#include <array>
#include <cmath>
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

// Aspect-lock presets (ADR-0162). The ids are the menu destination ids and
// dispatch keys; keep them stable, the rename dialog and saved state may
// reference them in later slices.
struct AspectRatioPreset
{
    QString id;
    QString label;
    double ratio;
};

constexpr double AspectRatioMatchTolerance = 0.01;

const std::array<AspectRatioPreset, 4> &aspectRatioPresets()
{
    static const std::array<AspectRatioPreset, 4> presets{{
        {QStringLiteral("16-9"), QStringLiteral("16:9"), 16.0 / 9.0},
        {QStringLiteral("4-3"), QStringLiteral("4:3"), 4.0 / 3.0},
        {QStringLiteral("21-9"), QStringLiteral("21:9"), 21.0 / 9.0},
        {QStringLiteral("1-1"), QStringLiteral("1:1"), 1.0},
    }};
    return presets;
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
        .activeMemberId = {},
        .canMinimize = window.isMinimizable(),
        .shaded = false,
        .containerName = {},
        .containerColors = {},
        .keepAbove = window.keepAbove(),
        .keepBelow = window.keepBelow(),
        .pinnedToAllWorkspaces = window.isOnAllDesktops(),
        .onAllActivities = window.isOnAllActivities(),
        .workspaces = {},
        .activities = {},
        .outputs = {},
        .aspectRatios = {},
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
    case GroupContextMenuCommandKind::SavedWorkspaces:
    case GroupContextMenuCommandKind::ArrangeWindows:
    case GroupContextMenuCommandKind::DetachActiveWindow:
    case GroupContextMenuCommandKind::Ungroup:
    case GroupContextMenuCommandKind::MinimizeGroup:
    case GroupContextMenuCommandKind::ToggleShadeGroup:
    case GroupContextMenuCommandKind::RenameContainer:
    case GroupContextMenuCommandKind::SetContainerColor:
    case GroupContextMenuCommandKind::SetContainerAspectRatio:
        return fail(error, QStringLiteral("group action requires session policy"));
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
            auto state = contextState(*representative, error);
            if (!state) {
                return std::nullopt;
            }
            const auto memberIds = m_runtime->topology().windowIds(containerId);
            state->canMinimize = !memberIds.isEmpty()
                && std::all_of(memberIds.cbegin(), memberIds.cend(),
                               [this](const QString &windowId) {
                                   const auto *window = m_registry.window(windowId);
                                   return window && window->isMinimizable();
                               });
            state->shaded = isContainerShaded(containerId);
            const auto appearance = containerAppearance(containerId);
            state->containerName = appearance.name;
            state->containerColors.append(
                {QStringLiteral("default"), QStringLiteral("Default"),
                 appearance.colorHex.isEmpty()});
            for (const auto &swatch : Compositor::containerColorSwatches()) {
                state->containerColors.append(
                    {swatch.colorHex, swatch.label,
                     swatch.colorHex == appearance.colorHex});
            }
            const auto pin = m_placement
                ? m_placement->aspectRatioPin(containerId) : std::nullopt;
            state->aspectRatios.append(
                {QStringLiteral("unlocked"), QStringLiteral("Unlocked"),
                 !pin.has_value()});
            state->aspectRatios.append(
                {QStringLiteral("current"), QStringLiteral("Lock current"), false});
            for (const auto &preset : aspectRatioPresets()) {
                state->aspectRatios.append(
                    {preset.id, preset.label,
                     pin.has_value()
                         && std::abs(*pin - preset.ratio)
                             <= AspectRatioMatchTolerance});
            }
            const QString activeId = m_registry.windowId(
                KWin::workspace()->activeWindow());
            state->activeMemberId =
                m_registry.owner(activeId) == containerId ? activeId
                                                          : representativeId;
            return state;
        },
        [this](const QString &containerId,
               const GroupContextMenuCommand &command,
               QString *error) {
            switch (command.kind) {
            case GroupContextMenuCommandKind::SavedWorkspaces:
                if (!ready() || !m_workspaceController) {
                    return fail(error, QStringLiteral("Saved workspaces are unavailable."));
                }
                showSavedWorkspaces(containerId);
                return true;
            case GroupContextMenuCommandKind::ArrangeWindows:
                return beginArrangeWindows(containerId, command.destinationId, error);
            case GroupContextMenuCommandKind::DetachActiveWindow:
                if (!restoreMemberFocusForInteraction(error)) {
                    return false;
                }
                return detachNativeMember(containerId, command.destinationId, error);
            case GroupContextMenuCommandKind::Ungroup:
                return ungroupContainer(containerId, error);
            case GroupContextMenuCommandKind::MinimizeGroup: {
                const auto memberIds = m_runtime->topology().windowIds(containerId);
                if (memberIds.isEmpty()
                    || !std::all_of(memberIds.cbegin(), memberIds.cend(),
                                    [this](const QString &windowId) {
                                        const auto *window = m_registry.window(windowId);
                                        return window && window->isMinimizable();
                                    })) {
                    return fail(error,
                                QStringLiteral("group can no longer be minimized"));
                }
                return dispatchGroupWindowAction(
                    containerId, HybridChrome::WindowAction::Minimize, error);
            }
            case GroupContextMenuCommandKind::ToggleShadeGroup: {
                if (!restoreMemberFocusForContainerAction(containerId, error)) {
                    return false;
                }
                return isContainerShaded(containerId)
                    ? unshadeContainer(containerId, error)
                    : shadeContainer(containerId, error);
            }
            case GroupContextMenuCommandKind::RenameContainer: {
                // AGENT-GUARD: Runs after the popup finishes hiding (see the
                // menu's hideEvent/schedulePendingDispatches contract), so
                // this modal prompt cannot stack a second popup over an
                // in-flight QMenu close animation.
                bool accepted = false;
                const QString current = containerAppearance(containerId).name;
                const QString entered = QInputDialog::getText(
                    nullptr, QObject::tr("Rename Window Group"),
                    QObject::tr("Group name:"), QLineEdit::Normal, current,
                    &accepted);
                if (!accepted) {
                    return true;
                }
                return renameContainer(containerId, entered, error);
            }
            case GroupContextMenuCommandKind::SetContainerColor: {
                const QString colorHex =
                    command.destinationId == QLatin1StringView("default")
                        ? QString{} : command.destinationId;
                return setContainerColor(containerId, colorHex, error);
            }
            case GroupContextMenuCommandKind::SetContainerAspectRatio:
                return applyAspectRatioSelection(containerId,
                                                 command.destinationId, error);
            default:
                break;
            }
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
    m_groupContextMenu->setPalette(m_nativePalette);
}

bool KWinHybridSession::applyAspectRatioSelection(const QString &containerId,
                                                  const QString &selectionId,
                                                  QString *error)
{
    if (!m_placement) {
        return fail(error, QStringLiteral("container placement is unavailable"));
    }
    if (selectionId == QLatin1StringView("unlocked")) {
        return m_placement->setAspectRatioPin(containerId, std::nullopt, error);
    }
    if (selectionId == QLatin1StringView("current")) {
        const auto layout = m_sceneFactory
            ? m_sceneFactory->committedLayout(containerId) : std::nullopt;
        if (!layout || !layout->outerFrame.isValid()) {
            return fail(error,
                        QStringLiteral("container has no committed frame to lock"));
        }
        const double ratio =
            HybridContainerPlacementController::contentAspectRatioForOuterFrame(
                layout->outerFrame);
        if (!(ratio > 0.0)) {
            return fail(error,
                        QStringLiteral("container frame has no lockable ratio"));
        }
        return m_placement->setAspectRatioPin(containerId, ratio, error);
    }
    for (const auto &preset : aspectRatioPresets()) {
        if (preset.id == selectionId) {
            return m_placement->setAspectRatioPin(containerId, preset.ratio,
                                                  error);
        }
    }
    return fail(error, QStringLiteral("unknown aspect-ratio selection"));
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

void KWinHybridSession::handleContainerControl(
    const QString &containerId,
    HybridChrome::ContainerControl control)
{
    QString error;
    if (!dispatchContainerControl(containerId, control, &error)) {
        qWarning("QindaQt window group control failed: %s", qPrintable(error));
    }
}

bool KWinHybridSession::dispatchContainerControl(
    const QString &containerId,
    HybridChrome::ContainerControl control,
    QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        return fail(error, QStringLiteral("the selected window group is stale"));
    }
    switch (control) {
    case HybridChrome::ContainerControl::ToggleMemberTitles: {
        if (!m_memberChromeVisibility) {
            return fail(error, QStringLiteral("native title controls are unavailable"));
        }
        MemberChromeVisibilitySummary summary;
        if (!m_memberChromeVisibility->toggle(containerId, &summary, error)) {
            return false;
        }
        if (summary.unchangedClientDecoratedMembers > 0) {
            qInfo("QindaQt left %lld client-drawn title bar(s) unchanged",
                  static_cast<long long>(
                      summary.unchangedClientDecoratedMembers));
        }
        synchronizeChrome();
        return true;
    }
    case HybridChrome::ContainerControl::ToggleShade: {
        if (!restoreMemberFocusForContainerAction(containerId, error)) {
            return false;
        }
        return isContainerShaded(containerId)
            ? unshadeContainer(containerId, error)
            : shadeContainer(containerId, error);
    }
    case HybridChrome::ContainerControl::ManagementMenu: {
        if (!m_groupContextMenu || !m_chromeManager) {
            return fail(error, QStringLiteral("window group actions are unavailable"));
        }
        const auto plan = m_chromeManager->plan(containerId);
        if (!plan) {
            return fail(error, QStringLiteral("window group controls are stale"));
        }
        const auto match = std::find_if(
            plan->controls.cbegin(), plan->controls.cend(),
            [control](const auto &candidate) {
                return candidate.control == control;
            });
        if (match == plan->controls.cend()) {
            return fail(error, QStringLiteral("window group menu control is absent"));
        }
        return m_groupContextMenu->popupForContainer(
            containerId, match->rect.bottomLeft(), error);
    }
    }
    return fail(error, QStringLiteral("unknown window group control"));
}


bool KWinHybridSession::beginArrangeWindows(const QString &containerId,
                                            const QString &windowId,
                                            QString *error)
{
    if (!ready() || !m_inputFilter || !m_inputFilter->installed()) {
        if (error) {
            *error = QStringLiteral("window arrangement input is unavailable");
        }
        return false;
    }
    const auto *container = m_runtime->topology().container(containerId);
    if (!container || !container->findWindow(windowId)
        || m_registry.owner(windowId) != containerId) {
        if (error) {
            *error = QStringLiteral("the selected group member is stale");
        }
        return false;
    }
    if (!restoreMemberFocusForInteraction(error)) {
        return false;
    }
    const HybridInput::HitTarget source{
        HybridInput::HitKind::MemberTitle, containerId, windowId, {}};
    if (m_inputFilter->beginKeyboardDock(source)) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("window arrangement could not acquire input");
    }
    return false;
}

bool KWinHybridSession::ungroupContainer(const QString &containerId,
                                         QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        if (error) {
            *error = QStringLiteral("the selected window group is stale");
        }
        return false;
    }
    if (!restoreMemberFocusForInteraction(error)) {
        return false;
    }
    const auto result = m_runtime->releaseContainer(containerId);
    if (!result.topologyChanged()) {
        if (error) {
            *error = result.message.isEmpty()
                ? QStringLiteral("the window group could not be released")
                : result.message;
        }
        return false;
    }
    forgetShadedContainer(containerId);
    m_placement->forgetContainer(containerId);
    m_minimizedContainers.remove(containerId);
    m_appearance.forgetContainer(containerId);
    synchronizeChrome();
    return true;
}

} // namespace QindaQt::Compositor::KWinIntegration
