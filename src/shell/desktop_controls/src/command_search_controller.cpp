// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/command_search_controller.h"

#include "command_search_targets.h"
#include "launcher_applet_controller.h"
#include "qindaqt/shell/global_menu/applet/globalmenuappletaccess.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::DesktopControls {

CommandSearchController::CommandSearchController(Sources sources, Grants grants,
                                                 QList<CommandSourceKind> enabledSources,
                                                 QObject *parent)
    : QObject(parent)
    , m_sources(sources)
    , m_grants(grants)
    , m_enabled(std::move(enabledSources))
{
  // Each facade marks only its own source stale (ADR-0391). A source this
  // instance does not consult is ignored inside sourceChanged(), so the HUD,
  // which reads menu actions only, no longer reacts to every task-list
  // publication or launcher change.
  if (m_sources.launcher != nullptr) {
    connect(m_sources.launcher, &Launcher::LauncherAppletController::stateChanged,
            this, [this] { sourceChanged(CommandSourceKind::Applications); });
  }
  if (m_sources.globalMenu != nullptr) {
    connect(m_sources.globalMenu, &GlobalMenu::GlobalMenuAppletAccess::itemsChanged,
            this, [this] { sourceChanged(CommandSourceKind::MenuActions); });
    connect(m_sources.globalMenu,
            &GlobalMenu::GlobalMenuAppletAccess::availableChanged, this,
            [this] { sourceChanged(CommandSourceKind::MenuActions); });
  }
  if (m_sources.taskList != nullptr) {
    connect(m_sources.taskList,
            &ShellTaskListApplet::TaskListAppletController::stateReprojected, this,
            [this] { sourceChanged(CommandSourceKind::Windows); });
  }
  if (m_sources.workspaces != nullptr) {
    connect(m_sources.workspaces, &Workspaces::WorkspaceController::stateChanged,
            this, [this] { sourceChanged(CommandSourceKind::Workspaces); });
  }
}

void CommandSearchController::setQuery(const QString &query)
{
  const QString bounded = query.left(CommandSearchBounds::maxQueryLength);
  if (bounded == m_query) {
    return;
  }
  m_query = bounded;
  Q_EMIT queryChanged();
  // Only the launcher's candidates depend on the query text; every source's
  // candidates are re-ranked against it.
  sourceChanged(CommandSourceKind::Applications);
}

void CommandSearchController::present()
{
  if (++m_presentations != 1) {
    return;
  }
  Q_EMIT presentedChanged();
  // The facades moved on while nothing was showing; catch up exactly once.
  if (m_staleSources != 0) {
    publishRefresh();
  }
}

void CommandSearchController::dismiss()
{
  if (m_presentations == 0) {
    return;
  }
  if (--m_presentations == 0) {
    Q_EMIT presentedChanged();
  }
}

void CommandSearchController::sourceChanged(CommandSourceKind kind)
{
  if (!m_enabled.contains(kind)) {
    return;
  }
  m_staleSources |= 1u << static_cast<unsigned>(kind);
  if (presented()) {
    publishRefresh();
  }
}

void CommandSearchController::ensureFresh() const
{
  if (m_staleSources != 0) {
    refreshSources();
  }
}

void CommandSearchController::publishRefresh()
{
  refreshSources();
  Q_EMIT resultsChanged();
  Q_EMIT stateChanged();
}

int CommandSearchController::resultCount() const
{
  ensureFresh();
  return static_cast<int>(m_results.size());
}

const QList<CommandCandidate> &CommandSearchController::rankedCandidates() const
{
  ensureFresh();
  return m_results;
}

QVariantList CommandSearchController::results() const
{
  ensureFresh();
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
  ensureFresh();
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

void CommandSearchController::refreshSources() const
{
  QList<CommandCandidate> candidates;
  for (const CommandSourceKind kind : m_enabled) {
    const unsigned bit = 1u << static_cast<unsigned>(kind);
    QList<CommandCandidate> &cached = m_sourceCandidates[static_cast<int>(kind)];
    if (!sourceActive(kind)) {
      cached.clear();
      continue;
    }
    if ((m_staleSources & bit) != 0) {
      switch (kind) {
      case CommandSourceKind::Applications:
        cached = collectApplications(m_query);
        break;
      case CommandSourceKind::MenuActions:
        cached = collectMenuActions();
        break;
      case CommandSourceKind::Windows:
        cached = collectWindows();
        break;
      case CommandSourceKind::Workspaces:
        cached = collectWorkspaces();
        break;
      }
      ++m_collections;
    }
    candidates.append(cached);
  }
  m_staleSources = 0;
  m_results = rankCommands(m_query, candidates);
}

void CommandSearchController::rebuild()
{
  m_staleSources = 0xFu;
  publishRefresh();
}

void CommandSearchController::refresh()
{
  rebuild();
}

bool CommandSearchController::activate(const QString &resultId,
                                        const QString &renderedGeneration)
{
  ensureFresh();
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
