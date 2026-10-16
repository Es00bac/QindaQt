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
//
// AGENT-CONTRACT (ADR-0391): the controller does work only for a surface that
// is showing it. The task list reprojects ten or more times a second under a
// title spinner, and the launcher projects the whole application catalog on
// every consultation, so a facade change while nothing is presented only
// marks the affected source stale: no collection, no ranking, no signal. A
// surface calls present() when it shows the results and dismiss() when it
// hides them (a counted lease: one controller serves every panel instance).
// The first present() brings the cache up to date and notifies once; while
// presented, each facade change refreshes just the source that changed. Any
// read (results, resultCount, phaseText, rankedCandidates, activate) refreshes
// a stale cache first, so a caller that never presents still reads current
// truth; only the change SIGNALS depend on presentation.
class CommandSearchController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
  Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
  Q_PROPERTY(int resultCount READ resultCount NOTIFY resultsChanged)
  Q_PROPERTY(bool presented READ presented NOTIFY presentedChanged)
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
  [[nodiscard]] int resultCount() const;
  // True while at least one surface holds the presentation lease.
  [[nodiscard]] bool presented() const noexcept { return m_presentations > 0; }
  // Counted presentation lease; see the class contract. dismiss() without a
  // matching present() is ignored.
  Q_INVOKABLE void present();
  Q_INVOKABLE void dismiss();
  // Full source collections performed so far; diagnostics and tests only.
  [[nodiscard]] int collectionCount() const noexcept { return m_collections; }
  [[nodiscard]] QString phaseText() const;
  [[nodiscard]] QString phaseReasonText() const;
  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] QStringList sourceKinds() const;
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }
  [[nodiscard]] const QList<CommandCandidate> &rankedCandidates() const;

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
  void presentedChanged();

private:
  [[nodiscard]] bool sourceActive(CommandSourceKind kind) const;
  // Marks one source stale; refreshes and notifies at once only while
  // presented. A kind this instance does not consult is ignored outright.
  void sourceChanged(CommandSourceKind kind);
  void ensureFresh() const;
  void refreshSources() const;
  void publishRefresh();
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
  QString m_feedback;
  int m_presentations = 0;
  // AGENT-GUARD: everything below is a derived cache of the borrowed facades,
  // mutable so a const read can bring it up to date without a signal. A set
  // bit in m_staleSources means that source's candidates must be collected
  // again; all four start stale because nothing has been collected yet.
  mutable unsigned m_staleSources = 0xFu;
  mutable QList<CommandCandidate> m_sourceCandidates[4];
  mutable QList<CommandCandidate> m_results;
  mutable int m_collections = 0;
};

} // namespace QindaQt::Shell::DesktopControls
