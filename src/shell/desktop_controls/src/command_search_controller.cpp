// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/command_search_controller.h"

#include "launcher_applet_controller.h"
#include "qindaqt/shell/global_menu/applet/globalmenuappletaccess.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::DesktopControls {
namespace {

constexpr auto ShowDesktopTarget = "show-desktop";

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

CommandSearchController::CommandSearchController(Sources sources, Grants grants,
                                                 QList<CommandSourceKind> enabledSources,
                                                 QObject *parent)
    : QObject(parent)
    , m_sources(sources)
    , m_grants(grants)
    , m_enabled(std::move(enabledSources))
{
  if (m_sources.launcher != nullptr) {
    connect(m_sources.launcher, &Launcher::LauncherAppletController::stateChanged,
            this, &CommandSearchController::rebuild);
  }
  if (m_sources.globalMenu != nullptr) {
    connect(m_sources.globalMenu, &GlobalMenu::GlobalMenuAppletAccess::itemsChanged,
            this, &CommandSearchController::rebuild);
    connect(m_sources.globalMenu,
            &GlobalMenu::GlobalMenuAppletAccess::availableChanged, this,
            &CommandSearchController::rebuild);
  }
  if (m_sources.taskList != nullptr) {
    connect(m_sources.taskList,
            &ShellTaskListApplet::TaskListAppletController::stateReprojected, this,
            &CommandSearchController::rebuild);
  }
  if (m_sources.workspaces != nullptr) {
    connect(m_sources.workspaces, &Workspaces::WorkspaceController::stateChanged,
            this, &CommandSearchController::rebuild);
  }
  rebuild();
}

void CommandSearchController::setQuery(const QString &query)
{
  const QString bounded = query.left(CommandSearchBounds::maxQueryLength);
  if (bounded == m_query) {
    return;
  }
  m_query = bounded;
  Q_EMIT queryChanged();
  rebuild();
}

QVariantList CommandSearchController::results() const
{
  QVariantList rows;
  rows.reserve(m_results.size());
  int index = 0;
  for (const CommandCandidate &candidate : m_results) {
    rows.append(QVariantMap{
        {QStringLiteral("id"), candidate.id},
        {QStringLiteral("kind"), commandSourceKindText(candidate.kind)},
        {QStringLiteral("text"), candidate.text},
        {QStringLiteral("detail"), candidate.detail},
        {QStringLiteral("iconName"), candidate.iconName},
        {QStringLiteral("accessibleName"), candidate.accessibleName},
        {QStringLiteral("enabled"), candidate.enabled},
        {QStringLiteral("index"), index},
        {QStringLiteral("generation"),
         candidate.kind == CommandSourceKind::MenuActions
             ? QString::number(candidate.revision)
             : QString{}},
    });
    ++index;
  }
  return rows;
}

bool CommandSearchController::sourceActive(CommandSourceKind kind) const
{
  if (!m_enabled.contains(kind)) {
    return false;
  }
  switch (kind) {
  case CommandSourceKind::Applications:
    return m_sources.launcher != nullptr && m_grants.applicationsLaunch;
  case CommandSourceKind::MenuActions:
    return m_sources.globalMenu != nullptr && m_grants.globalMenuRead;
  case CommandSourceKind::Windows:
    return m_sources.taskList != nullptr && m_grants.windowsRead;
  case CommandSourceKind::Workspaces:
    return m_sources.workspaces != nullptr && m_grants.windowsRead;
  }
  return false;
}

bool CommandSearchController::available() const noexcept
{
  for (const CommandSourceKind kind : m_enabled) {
    if (sourceActive(kind)) {
      return true;
    }
  }
  return false;
}

QString CommandSearchController::phaseText() const
{
  if (!available()) {
    return QStringLiteral("unavailable");
  }
  return m_results.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
}

QString CommandSearchController::phaseReasonText() const
{
  QStringList missing;
  for (const CommandSourceKind kind : m_enabled) {
    if (!sourceActive(kind)) {
      missing.append(commandSourceKindText(kind));
    }
  }
  if (missing.isEmpty()) {
    return {};
  }
  return QStringLiteral("sources-unavailable:%1").arg(missing.join(QLatin1Char(',')));
}

QStringList CommandSearchController::sourceKinds() const
{
  QStringList kinds;
  for (const CommandSourceKind kind : m_enabled) {
    kinds.append(commandSourceKindText(kind));
  }
  return kinds;
}

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
    candidate.revision = row.value(QStringLiteral("generationRevision")).toULongLong();
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

void CommandSearchController::rebuild()
{
  QList<CommandCandidate> candidates;
  for (const CommandSourceKind kind : m_enabled) {
    if (!sourceActive(kind)) {
      continue;
    }
    switch (kind) {
    case CommandSourceKind::Applications:
      candidates.append(collectApplications(m_query));
      break;
    case CommandSourceKind::MenuActions:
      candidates.append(collectMenuActions());
      break;
    case CommandSourceKind::Windows:
      candidates.append(collectWindows());
      break;
    case CommandSourceKind::Workspaces:
      candidates.append(collectWorkspaces());
      break;
    }
  }
  m_results = rankCommands(m_query, candidates);
  Q_EMIT resultsChanged();
  Q_EMIT stateChanged();
}

void CommandSearchController::refresh()
{
  rebuild();
}

bool CommandSearchController::activate(const QString &resultId,
                                        const QString &renderedGeneration)
{
  for (const CommandCandidate &candidate : m_results) {
    if (candidate.id != resultId) {
      continue;
    }
    if (!candidate.enabled) {
      publishFeedback(QStringLiteral("%1 is not available right now").arg(candidate.text));
      return false;
    }
    return activateCandidate(candidate, renderedGeneration);
  }
  publishFeedback(QStringLiteral("That result is no longer listed"));
  return false;
}

bool CommandSearchController::activateCandidate(const CommandCandidate &candidate,
                                                const QString &renderedGeneration)
{
  if (!sourceActive(candidate.kind)) {
    publishFeedback(QStringLiteral("That source is unavailable"));
    return false;
  }
  bool ok = false;
  switch (candidate.kind) {
  case CommandSourceKind::Applications:
    ok = m_sources.launcher->activate(candidate.targetId);
    if (!ok) {
      publishFeedback(m_sources.launcher->feedback().isEmpty()
                          ? QStringLiteral("Could not start %1").arg(candidate.text)
                          : m_sources.launcher->feedback());
    }
    break;
  case CommandSourceKind::MenuActions: {
    if (!m_sources.globalMenu->available()) {
      publishFeedback(QStringLiteral("The application menu is no longer available"));
      break;
    }
    // AGENT-GUARD: the displayed row must still belong to the same menu
    // publication. A republished tree can reuse an action id (and even the
    // same label), so comparing only the current candidate would let a stale
    // QML row invoke a newer action. The facade repeats this generation check
    // at its own boundary before emitting activationRequested.
    const QString currentGeneration = QString::number(candidate.revision);
    if (!renderedGeneration.isEmpty() && renderedGeneration != currentGeneration) {
      publishFeedback(QStringLiteral("The application menu changed; search again"));
      rebuild();
      break;
    }
    if (candidate.revision == 0) {
      publishFeedback(QStringLiteral("The application menu has no valid publication"));
      break;
    }
    bool stillCurrent = false;
    for (const CommandCandidate &current : collectMenuActions()) {
      if (current.id == candidate.id && current.text == candidate.text
          && current.detail == candidate.detail && current.enabled) {
        stillCurrent = true;
        break;
      }
    }
    if (!stillCurrent) {
      publishFeedback(QStringLiteral("The application menu changed; search again"));
      rebuild();
      break;
    }
    m_sources.globalMenu->activate(candidate.targetId, currentGeneration);
    ok = true;
    break;
  }
  case CommandSourceKind::Windows:
    if (!m_grants.windowsActivate) {
      publishFeedback(QStringLiteral("Window activation is not granted"));
      break;
    }
    ok = m_sources.taskList->activateTask(candidate.targetId, candidate.revision);
    if (!ok && m_sources.taskList->feedbackPresent()) {
      publishFeedback(m_sources.taskList->feedback());
    }
    break;
  case CommandSourceKind::Workspaces:
    if (!m_grants.windowsManage) {
      publishFeedback(QStringLiteral("Workspace control is not granted"));
      break;
    }
    if (candidate.targetId == QLatin1StringView(ShowDesktopTarget)) {
      ok = m_sources.workspaces->toggleShowingDesktop();
    } else {
      ok = m_sources.workspaces->switchTo(candidate.targetId, candidate.revision);
    }
    if (!ok && m_sources.workspaces->feedbackPresent()) {
      publishFeedback(m_sources.workspaces->feedback());
    }
    break;
  }
  if (ok) {
    clearFeedback();
  }
  return ok;
}

void CommandSearchController::clearFeedback()
{
  publishFeedback({});
}

void CommandSearchController::publishFeedback(const QString &message)
{
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopControls
