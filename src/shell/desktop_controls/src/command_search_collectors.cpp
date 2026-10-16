// SPDX-License-Identifier: GPL-3.0-or-later
//
// The four source collectors of CommandSearchController: how a launcher section,
// a global-menu tree, a task-list row and a workspace become ranked-search
// candidates. Split from command_search_controller.cpp (source-shape review)
// so that file keeps the lease, cache and activation logic and this one keeps
// the per-source projection. They are members of the same class; the cache that
// decides WHEN to call them is in the controller (ADR-0391).
#include "qindaqt/shell/desktop_controls/command_search_controller.h"

#include "command_search_targets.h"
#include "launcher_applet_controller.h"
#include "qindaqt/shell/global_menu/applet/globalmenuappletaccess.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::DesktopControls {
namespace {

QString boundedText(const QString &text)
{
  return text.left(CommandSearchBounds::maxTextLength);
}

// Depth-first walk of the global-menu projection collecting enabled actions
// with a breadcrumb detail. Depth and count are bounded; hidden entries never
// reach the projection in the first place.
void collectMenuActionsFrom(const QVariantList &items, const QString &path,
                            int depth, QList<CommandCandidate> &out)
{
  if (depth > 8) {
    return;
  }
  for (const QVariant &value : items) {
    if (out.size() >= CommandSearchBounds::maxCandidatesPerSource) {
      return;
    }
    const QVariantMap item = value.toMap();
    const QString kind = item.value(QStringLiteral("kind")).toString();
    const QString text = boundedText(item.value(QStringLiteral("text")).toString());
    if (kind == QLatin1StringView("submenu")) {
      const QString nextPath = path.isEmpty()
          ? text
          : QStringLiteral("%1 › %2").arg(path, text);
      collectMenuActionsFrom(item.value(QStringLiteral("children")).toList(),
                             nextPath, depth + 1, out);
      continue;
    }
    if (kind != QLatin1StringView("action") || text.isEmpty()) {
      continue;
    }
    CommandCandidate candidate;
    candidate.kind = CommandSourceKind::MenuActions;
    candidate.targetId = item.value(QStringLiteral("id")).toString();
    candidate.id = QStringLiteral("menuAction:") + candidate.targetId;
    candidate.revision = item.value(QStringLiteral("generation")).toULongLong();
    candidate.text = text;
    candidate.detail = path;
    candidate.enabled = item.value(QStringLiteral("enabled")).toBool();
    const QString shortcut = item.value(QStringLiteral("shortcutText")).toString();
    candidate.accessibleName = shortcut.isEmpty()
        ? QStringLiteral("%1, %2").arg(text, path)
        : QStringLiteral("%1, %2, %3").arg(text, path, shortcut);
    candidate.order = static_cast<int>(out.size());
    out.append(std::move(candidate));
  }
}

} // namespace

QList<CommandCandidate> CommandSearchController::collectApplications(
    const QString &query) const
{
  QList<CommandCandidate> out;
  const QVariantList sections = m_sources.launcher->sectionsForQuery(query);
  const bool browse = normalizeCommandQuery(query).isEmpty();
  for (const QVariant &sectionValue : sections) {
    const QVariantMap section = sectionValue.toMap();
    const QString kind = section.value(QStringLiteral("kind")).toString();
    // Browsing (empty query) offers only pinned and recent entries, mirroring
    // the launcher's own default view; a category dump is not a useful palette.
    if (browse && kind != QLatin1StringView("pinned")
        && kind != QLatin1StringView("recent")) {
      continue;
    }
    for (const QVariant &itemValue : section.value(QStringLiteral("items")).toList()) {
      if (out.size() >= CommandSearchBounds::maxCandidatesPerSource) {
        return out;
      }
      const QVariantMap item = itemValue.toMap();
      CommandCandidate candidate;
      candidate.kind = CommandSourceKind::Applications;
      candidate.targetId = item.value(QStringLiteral("entryId")).toString();
      candidate.id = QStringLiteral("application:") + candidate.targetId;
      candidate.text = boundedText(item.value(QStringLiteral("displayText")).toString());
      candidate.detail = kind == QLatin1StringView("recent")
          ? QStringLiteral("Recent application")
          : kind == QLatin1StringView("pinned") ? QStringLiteral("Pinned application")
                                                : QStringLiteral("Application");
      candidate.iconName = item.value(QStringLiteral("iconName")).toString();
      candidate.enabled = m_sources.launcher->launchGranted();
      candidate.accessibleName = QStringLiteral("%1, %2").arg(candidate.text, candidate.detail);
      candidate.order = static_cast<int>(out.size());
      bool duplicate = false;
      for (const CommandCandidate &existing : out) {
        duplicate = duplicate || existing.id == candidate.id;
      }
      if (!duplicate) {
        out.append(std::move(candidate));
      }
    }
  }
  return out;
}

QList<CommandCandidate> CommandSearchController::collectMenuActions() const
{
  QList<CommandCandidate> out;
  if (!m_sources.globalMenu->available()) {
    return out;
  }
  collectMenuActionsFrom(m_sources.globalMenu->items(), QString{}, 0, out);
  return out;
}

QList<CommandCandidate> CommandSearchController::collectWindows() const
{
  QList<CommandCandidate> out;
  const bool canActivate = m_grants.windowsActivate && m_sources.taskList->canActivate();
  const quint64 generation = m_sources.taskList->generationRevision();
  for (const QVariant &value : m_sources.taskList->entryRows()) {
    if (out.size() >= CommandSearchBounds::maxCandidatesPerSource) {
      break;
    }
    const QVariantMap row = value.toMap();
    CommandCandidate candidate;
    candidate.kind = CommandSourceKind::Windows;
    candidate.targetId = row.value(QStringLiteral("taskId")).toString();
    candidate.id = QStringLiteral("window:") + candidate.targetId;
    candidate.text = boundedText(row.value(QStringLiteral("title")).toString());
    const QString application = row.value(QStringLiteral("applicationName")).toString();
    candidate.detail = row.value(QStringLiteral("active")).toBool()
        ? QStringLiteral("%1, active window").arg(application)
        : row.value(QStringLiteral("minimized")).toBool()
            ? QStringLiteral("%1, minimized").arg(application)
            : application;
    candidate.iconName = row.value(QStringLiteral("iconName")).toString();
    candidate.enabled = canActivate && !row.value(QStringLiteral("pending")).toBool();
    // The generation the rows were read from (ADR-0390): the facade's, not a
    // per-row copy. Windows are collected from the same reprojection.
    candidate.revision = generation;
    candidate.accessibleName = QStringLiteral("%1, %2").arg(candidate.text, candidate.detail);
    candidate.order = static_cast<int>(out.size());
    out.append(std::move(candidate));
  }
  return out;
}

QList<CommandCandidate> CommandSearchController::collectWorkspaces() const
{
  QList<CommandCandidate> out;
  const Workspaces::WorkspaceController *workspaces = m_sources.workspaces;
  const bool canSwitch = m_grants.windowsManage && workspaces->canSwitch();
  for (const QVariant &value : workspaces->rows()) {
    const QVariantMap row = value.toMap();
    const bool current = row.value(QStringLiteral("current")).toBool();
    CommandCandidate candidate;
    candidate.kind = CommandSourceKind::Workspaces;
    candidate.targetId = row.value(QStringLiteral("id")).toString();
    candidate.id = QStringLiteral("workspace:") + candidate.targetId;
    candidate.text = QStringLiteral("Switch to %1")
                         .arg(boundedText(row.value(QStringLiteral("name")).toString()));
    candidate.detail = current ? QStringLiteral("Current workspace")
                               : QStringLiteral("Workspace");
    candidate.iconName = QStringLiteral("virtual-desktops");
    candidate.enabled = canSwitch && !current;
    candidate.revision = row.value(QStringLiteral("revision")).toULongLong();
    candidate.accessibleName = QStringLiteral("%1, %2").arg(candidate.text, candidate.detail);
    candidate.order = static_cast<int>(out.size());
    out.append(std::move(candidate));
  }
  if (m_grants.windowsManage && workspaces->phase() == Workspaces::WorkspacePhase::Ready) {
    CommandCandidate toggle;
    toggle.kind = CommandSourceKind::Workspaces;
    toggle.targetId = QString::fromLatin1(ShowDesktopTarget);
    toggle.id = QStringLiteral("workspace:") + toggle.targetId;
    toggle.text = workspaces->showingDesktop() ? QStringLiteral("Hide desktop")
                                               : QStringLiteral("Show desktop");
    toggle.detail = QStringLiteral("Desktop");
    toggle.iconName = QStringLiteral("user-desktop");
    toggle.enabled = workspaces->canShowDesktop();
    toggle.accessibleName = QStringLiteral("%1, %2").arg(toggle.text, toggle.detail);
    toggle.order = static_cast<int>(out.size());
    out.append(std::move(toggle));
  }
  return out;
}

} // namespace QindaQt::Shell::DesktopControls
