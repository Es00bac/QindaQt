// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_controls/command_search_types.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace QindaQt::Shell::Launcher {
class LauncherAppletController;
}
namespace QindaQt::Shell::GlobalMenu {
class GlobalMenuAppletAccess;
}
namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
}
namespace QindaQt::Shell::Workspaces {
class WorkspaceController;
}

namespace QindaQt::Shell::DesktopControls {

// Unified ranked search over borrowed facades. One instance serves one
// presentation: the command palette (all sources), the HUD (menu actions
// only), and the overview (windows, workspaces, applications). It owns query
// state and the last ranked list; every activation re-enters the owning
// facade with the identity and revision that facade published.
//
// AGENT-CONTRACT: every borrowed facade may be null and must otherwise
// outlive this controller on the GUI thread. A source is consulted only when
// its facade exists, its grant is held, and its kind is enabled here. The
// controller never caches an activation target beyond the current result
// list, so a stale row can never be dispatched with a newer revision.
class CommandSearchController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
  Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
  Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)
  Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateChanged)
  Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY stateChanged)
  Q_PROPERTY(bool available READ available NOTIFY stateChanged)
  Q_PROPERTY(QStringList sourceKinds READ sourceKinds CONSTANT)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  struct Sources {
    Launcher::LauncherAppletController *launcher = nullptr;
    GlobalMenu::GlobalMenuAppletAccess *globalMenu = nullptr;
    ShellTaskListApplet::TaskListAppletController *taskList = nullptr;
    Workspaces::WorkspaceController *workspaces = nullptr;
  };

  struct Grants {
    bool applicationsLaunch = false;
    bool globalMenuRead = false;
    bool windowsRead = false;
    bool windowsActivate = false;
    bool windowsManage = false;

    friend bool operator==(const Grants &, const Grants &) = default;
  };

  CommandSearchController(Sources sources, Grants grants,
                          QList<CommandSourceKind> enabledSources,
                          QObject *parent = nullptr);

  [[nodiscard]] QString query() const { return m_query; }
  void setQuery(const QString &query);
  // Rows: {id, kind, text, detail, iconName, accessibleName, enabled, index,
  // generation}. Menu rows carry the global-menu publication generation.
  [[nodiscard]] QVariantList results() const;
  [[nodiscard]] int resultCount() const noexcept
  {
    return static_cast<int>(m_results.size());
  }
  [[nodiscard]] QString phaseText() const;
  [[nodiscard]] QString phaseReasonText() const;
  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] QStringList sourceKinds() const;
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }
  [[nodiscard]] const QList<CommandCandidate> &rankedCandidates() const noexcept
  {
    return m_results;
  }

  // Activates one displayed result. QML supplies the generation carried by
  // its rendered menu row; an empty generation keeps the C++ convenience API
  // and still resolves the controller's current row synchronously.
  Q_INVOKABLE bool activate(const QString &resultId,
                           const QString &renderedGeneration = {});
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void queryChanged();
  void resultsChanged();
  void stateChanged();
  void feedbackChanged();

private:
  [[nodiscard]] bool sourceActive(CommandSourceKind kind) const;
  [[nodiscard]] QList<CommandCandidate> collectApplications(const QString &query) const;
  [[nodiscard]] QList<CommandCandidate> collectMenuActions() const;
  [[nodiscard]] QList<CommandCandidate> collectWindows() const;
  [[nodiscard]] QList<CommandCandidate> collectWorkspaces() const;
  [[nodiscard]] bool activateCandidate(const CommandCandidate &candidate,
                                       const QString &renderedGeneration);
  void rebuild();
  void publishFeedback(const QString &message);

  Sources m_sources;
  Grants m_grants;
  QList<CommandSourceKind> m_enabled;
  QString m_query;
  QList<CommandCandidate> m_results;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopControls
