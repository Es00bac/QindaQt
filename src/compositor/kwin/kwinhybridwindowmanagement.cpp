// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime.h"
#include "kwinhybridsession.h"
#include "kwinmemberpolicy.h"
#include "kwinsemanticwindowplacement.h"
#include "managedwindowregistry.h"
#include <QJsonArray>
#include <QUuid>
#include <qindaqt/window_management/grouping_planner.h>
#include <window.h>
#include <workspace.h>
namespace QindaQt::Compositor::KWinIntegration {
using namespace WindowManagement;
namespace {
Result outcome(bool accepted, QString message, const ResolvedTarget &target) {
  return {accepted ? Status::Accepted : Status::Unavailable,
          std::move(message),
          {},
          {},
          target.windowId,
          target.containerId};
}
} // namespace
Result KWinHybridSession::executeWindowManagementCommand(
    const Command &command, const ResolvedTarget &target,
    const ResolvedTarget &destination) {
  auto *window = m_registry.window(target.windowId);
  if (!ready() || !window || window->isDeleted() || !window->isNormalWindow() ||
      m_registry.owner(target.windowId) != target.containerId)
    return {Status::Stale,
            QStringLiteral("target ownership changed"),
            {},
            {},
            {},
            {}};
  QString error;
  bool accepted = false;
  const auto &containerId = target.containerId;
  const bool grouped = !containerId.isEmpty();
  auto *container =
      grouped ? m_runtime->topology().container(containerId) : nullptr;
  if (grouped && !container)
    return {
        Status::Stale, QStringLiteral("container disappeared"), {}, {}, {}, {}};
  const auto shell = [this, &target, &error, window,
                      grouped](ShellWindowAction action) {
    if (grouped)
      return executeShellWindowAction(target.windowId, action, &error);
    switch (action) {
    case ShellWindowAction::Activate:
      if (isWindowIconified(target.windowId) &&
          !restoreIconifiedWindow(target.windowId, true, &error))
        return false;
      window->setMinimized(false);
      KWin::workspace()->activateWindow(window, true);
      return true;
    case ShellWindowAction::Raise:
      KWin::workspace()->raiseWindow(window);
      return true;
    case ShellWindowAction::Minimize:
      if (!window->isMinimizable())
        return false;
      window->setMinimized(true);
      return true;
    case ShellWindowAction::Unminimize:
      window->setMinimized(false);
      return true;
    case ShellWindowAction::Close:
      if (!window->isCloseable())
        return false;
      window->closeWindow();
      return true;
    }
    return false;
  };
  switch (command.operation) {
  case Operation::Focus: {
    if (!grouped || command.target.kind == Target::Kind::Container) {
      accepted = shell(ShellWindowAction::Activate);
      break;
    }
    if (m_memberPolicy && !m_memberPolicy->focusStates().isEmpty()) {
      error = QStringLiteral(
          "restore temporary member focus before selecting a tab");
      break;
    }
    QString pageId;
    for (const auto &page : container->pages())
      if (page.root().findWindow(target.windowId))
        pageId = page.id();
    if (pageId.isEmpty() || !shell(ShellWindowAction::Activate))
      break;
    const auto activated = m_runtime->activatePage(containerId, pageId);
    if (!activated.topologyChanged() &&
        activated.status != HybridRuntimeStatus::NoChange) {
      error = activated.message;
      break;
    }
    // AGENT-GUARD: shell task activation intentionally focuses the container's
    // representative. A semantic window target must select its owning page
    // and exact leaf, including a non-representative tile (ADR-0301).
    auto *selected = m_registry.window(target.windowId);
    if (!selected || selected->isDeleted() ||
        m_registry.owner(target.windowId) != containerId)
      break;
    selected->setMinimized(false);
    KWin::workspace()->activateWindow(selected, true);
    accepted = true;
    break;
  }
  case Operation::Raise:
    accepted = shell(ShellWindowAction::Raise);
    break;
  case Operation::Minimize:
    accepted = shell(ShellWindowAction::Minimize);
    break;
  case Operation::Close:
    accepted = shell(ShellWindowAction::Close);
    break;
  case Operation::Rename:
    accepted =
        grouped &&
        renameContainer(
            containerId,
            command.arguments.value(QStringLiteral("name")).toString(), &error);
    break;
  case Operation::Color:
    accepted = grouped &&
               setContainerColor(
                   containerId,
                   command.arguments.value(QStringLiteral("value")).toString(),
                   &error);
    break;
  case Operation::Shade:
  case Operation::Iconify:
    accepted = grouped ? shadeContainer(containerId, &error)
                       : iconifyWindow(target.windowId, &error);
    break;
  case Operation::Unshade:
  case Operation::Uniconify:
    accepted =
        grouped ? (isContainerShaded(containerId)
                       ? unshadeContainer(containerId, &error)
                       : true)
                : (isWindowIconified(target.windowId)
                       ? restoreIconifiedWindow(target.windowId, true, &error)
                       : true);
    break;
  case Operation::Restore:
    if (grouped) {
      accepted = isContainerShaded(containerId)
                     ? unshadeContainer(containerId, &error)
                 : m_placement->isMaximized(containerId)
                     ? m_placement->restore(containerId, &error)
                     : shell(ShellWindowAction::Unminimize);
    } else if (isWindowIconified(target.windowId))
      accepted = restoreIconifiedWindow(target.windowId, true, &error);
    else if (m_semanticWindowPlacement &&
             m_semanticWindowPlacement->isMaximized(target.windowId)) {
      if (window->isMinimized())
        window->setMinimized(false);
      accepted = m_semanticWindowPlacement->restore(target.windowId, &error);
    } else {
      window->setFullScreen(false);
      window->maximize(KWin::MaximizeRestore);
      accepted = shell(ShellWindowAction::Unminimize);
    }
    break;
  case Operation::Maximize:
  case Operation::Place: {
    if ((m_memberPolicy && !m_memberPolicy->focusStates().isEmpty()) ||
        isWindowIconified(target.windowId))
      return {Status::Denied,
              QStringLiteral("restore temporary member focus or iconification "
                             "before placement"),
              {},
              {},
              {},
              {}};
    if (!m_semanticWindowPlacement)
      m_semanticWindowPlacement =
          std::make_unique<KWinSemanticWindowPlacement>(m_registry, this);
    if (command.operation == Operation::Maximize) {
      const auto fraction =
          command.arguments.value(QStringLiteral("fraction")).toDouble(0.9);
      accepted =
          grouped ? m_placement->maximizeFraction(containerId, fraction, &error)
                  : m_semanticWindowPlacement->maximize(target.windowId,
                                                        fraction, &error);
    } else {
      const auto region =
          command.arguments.value(QStringLiteral("region")).toArray();
      const auto area = grouped
                            ? workArea(containerId)
                            : QRect(KWin::workspace()
                                        ->clientArea(KWin::MaximizeArea, window)
                                        .toAlignedRect());
      const auto frame =
          regionalFrame(area, {region[0].toDouble(), region[1].toDouble(),
                               region[2].toDouble(), region[3].toDouble()});
      if (!frame)
        return {Status::Unavailable,
                QStringLiteral("no valid usable output region"),
                {},
                {},
                {},
                {}};
      accepted = grouped ? m_placement->placeFrame(containerId, *frame, &error)
                         : m_semanticWindowPlacement->place(target.windowId,
                                                            *frame, &error);
    }
    break;
  }
  case Operation::Fullscreen:
    if (grouped)
      return {
          Status::Unavailable,
          QStringLiteral(
              "whole-container fullscreen is unavailable; use full maximize"),
          {},
          {},
          {},
          {}};
    if (isWindowIconified(target.windowId) || !window->isFullScreenable())
      break;
    window->setFullScreen(
        command.arguments.value(QStringLiteral("enabled")).toBool());
    accepted = true;
    break;
  case Operation::Detach:
  case Operation::NextTab:
  case Operation::PreviousTab:
  case Operation::ActivateTab:
  case Operation::ReorderTab:
  case Operation::ResizeSplit: {
    if (!grouped ||
        (m_memberPolicy && !m_memberPolicy->focusStates().isEmpty()))
      break;
    std::optional<Hybrid::TopologyCommand> mutation;
    if (command.operation == Operation::Detach)
      mutation = Hybrid::DetachMember{containerId, target.windowId};
    else if (command.operation == Operation::ResizeSplit) {
      const auto split = m_runtime->activePageFirstSplitId(containerId);
      if (!split.isEmpty())
        mutation = Hybrid::ResizeSplit{
            containerId, split,
            command.arguments.value(QStringLiteral("ratio")).toDouble()};
    } else {
      qsizetype index = 0;
      for (qsizetype i = 0; i < container->pages().size(); ++i)
        if (container->pages()[i].id() == container->activePageId())
          index = i;
      if (command.operation == Operation::NextTab)
        index = (index + 1) % container->pages().size();
      else if (command.operation == Operation::PreviousTab)
        index =
            (index + container->pages().size() - 1) % container->pages().size();
      else
        index = command.arguments.value(QStringLiteral("index")).toInt() - 1;
      if (index < 0 || index >= container->pages().size())
        break;
      mutation = command.operation == Operation::ReorderTab
                     ? Hybrid::TopologyCommand(Hybrid::ReorderPage{
                           containerId, container->activePageId(), index})
                     : Hybrid::TopologyCommand(Hybrid::ActivatePage{
                           containerId, container->pages()[index].id()});
    }
    if (mutation) {
      const auto result = m_runtime->execute(*mutation);
      accepted = result.topologyChanged() ||
                 result.status == HybridRuntimeStatus::NoChange;
      error = result.message;
    }
    break;
  }
  case Operation::GroupTab:
  case Operation::GroupTile: {
    // AGENT-CONTRACT: semantic grouping moves one resolved leaf. An explicit
    // source container must not silently lose only its foreground member.
    if (command.target.kind == Target::Kind::Container) {
      error = QStringLiteral("select a window when grouping; whole-container "
                             "moves are unavailable");
      break;
    }
    auto *destinationWindow = m_registry.window(destination.windowId);
    if (!destinationWindow || destinationWindow->isDeleted() ||
        !destinationWindow->isNormalWindow() ||
        m_registry.owner(destination.windowId) != destination.containerId)
      return {Status::Stale,
              QStringLiteral("destination ownership changed"),
              {},
              {},
              {},
              {}};
    const auto temporarilyHidden = [this](const ResolvedTarget &leaf) {
      return isWindowIconified(leaf.windowId) ||
             (!leaf.containerId.isEmpty() &&
              (isContainerShaded(leaf.containerId) ||
               m_minimizedContainers.contains(leaf.containerId)));
    };
    // An inactive group page uses native minimization for visibility. It is
    // still a movable leaf; explicit whole-container minimization is fenced
    // by temporarilyHidden, and tile destinations must be the active page.
    if ((!grouped && window->isMinimized()) ||
        (destination.containerId.isEmpty() &&
         destinationWindow->isMinimized()) ||
        window->isFullScreen() || destinationWindow->isFullScreen() ||
        temporarilyHidden(target) || temporarilyHidden(destination) ||
        (m_memberPolicy && !m_memberPolicy->focusStates().isEmpty())) {
      error = QStringLiteral("restore temporary fullscreen, hidden or "
                             "member-focus presentation before grouping");
      break;
    }
    if (command.operation == Operation::GroupTile &&
        !destination.containerId.isEmpty()) {
      const auto *owner =
          m_runtime->topology().container(destination.containerId);
      const auto *page = owner ? owner->page(owner->activePageId()) : nullptr;
      if (!page || !page->root().findWindow(destination.windowId)) {
        error = QStringLiteral(
            "activate the destination page before adding a tile");
        break;
      }
    }
    GroupingDirection direction = GroupingDirection::Right;
    const auto requestedDirection =
        command.arguments.value(QStringLiteral("direction")).toString();
    if (requestedDirection == QStringLiteral("left"))
      direction = GroupingDirection::Left;
    else if (requestedDirection == QStringLiteral("up"))
      direction = GroupingDirection::Above;
    else if (requestedDirection == QStringLiteral("down"))
      direction = GroupingDirection::Below;
    const auto plan = planGrouping(
        m_runtime->topology(),
        {target.windowId, destination.windowId,
         command.operation == Operation::GroupTab ? GroupingMode::Tab
                                                  : GroupingMode::Tile,
         direction,
         command.arguments.value(QStringLiteral("ratio")).toDouble(0.5),
         QUuid::createUuid().toString(QUuid::WithoutBraces)});
    if (!plan.command) {
      error = plan.error;
      break;
    }
    // AGENT-GUARD: active-page choice is already part of this one scene
    // candidate. A second activation after publish would escape rollback.
    const auto result = m_runtime->execute(*plan.command);
    accepted = result.topologyChanged();
    error = result.message;
    break;
  }
  case Operation::Launch:
    break;
  }
  if (accepted) {
    synchronizeChrome();
    Q_EMIT shellVisibilityStateChanged();
  }
  if (!accepted && error.isEmpty())
    error = QStringLiteral("operation is unavailable for this target state");
  const ResolvedTarget finalTarget{target.windowId,
                                   m_registry.owner(target.windowId)};
  return outcome(accepted, error, finalTarget);
}
} // namespace QindaQt::Compositor::KWinIntegration
