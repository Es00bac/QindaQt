// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/workspaces/workspace_types.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <optional>

namespace QindaQt::Shell::Workspaces {

class WorkspaceTransport;

// Shell-private facade over the injected workspace transport for the compiled
// QindaQt.Shell.DesktopControls module. QML sees bounded rows, phase truth,
// and generation-fenced intents only; it never sees the bus, the compositor
// owner string, or the transport.
//
// AGENT-CONTRACT: the borrowed transport must outlive this controller and
// share its thread. Shell composition owns transport start/stop. At most one
// mutation is in flight; owner loss ends it as Uncertain and nothing is ever
// replayed. `revision` is a controller-local monotonic count of accepted
// snapshots and is the fence every switch intent must echo.
class WorkspaceController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateChanged)
  Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY stateChanged)
  Q_PROPERTY(QVariantList rows READ rows NOTIFY stateChanged)
  Q_PROPERTY(int count READ count NOTIFY stateChanged)
  Q_PROPERTY(QString currentId READ currentId NOTIFY stateChanged)
  Q_PROPERTY(QString currentName READ currentName NOTIFY stateChanged)
  Q_PROPERTY(int currentIndex READ currentIndex NOTIFY stateChanged)
  Q_PROPERTY(bool showingDesktop READ showingDesktop NOTIFY stateChanged)
  Q_PROPERTY(quint64 revision READ revision NOTIFY stateChanged)
  Q_PROPERTY(bool readGranted READ readGranted CONSTANT)
  Q_PROPERTY(bool manageGranted READ manageGranted CONSTANT)
  Q_PROPERTY(bool canSwitch READ canSwitch NOTIFY stateChanged)
  Q_PROPERTY(bool canShowDesktop READ canShowDesktop NOTIFY stateChanged)
  Q_PROPERTY(bool operationPending READ operationPending NOTIFY stateChanged)
  Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
  Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  WorkspaceController(WorkspaceTransport *transport, WorkspaceGrants grants,
                      QObject *parent = nullptr);

  [[nodiscard]] QString phaseText() const;
  [[nodiscard]] QString phaseReasonText() const { return m_phaseReason; }
  [[nodiscard]] WorkspacePhase phase() const noexcept { return m_phase; }
  // Rows: {id, name, position, index, current, accessibleName, revision}.
  [[nodiscard]] QVariantList rows() const;
  [[nodiscard]] int count() const noexcept;
  [[nodiscard]] QString currentId() const;
  [[nodiscard]] QString currentName() const;
  [[nodiscard]] int currentIndex() const;
  [[nodiscard]] bool showingDesktop() const noexcept;
  [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
  [[nodiscard]] bool readGranted() const noexcept { return m_grants.windowsRead; }
  [[nodiscard]] bool manageGranted() const noexcept { return m_grants.windowsManage; }
  [[nodiscard]] bool canSwitch() const noexcept;
  [[nodiscard]] bool canShowDesktop() const noexcept;
  [[nodiscard]] bool operationPending() const noexcept { return m_pending.has_value(); }
  [[nodiscard]] QString accessibleName() const;
  [[nodiscard]] QString accessibleDescription() const;
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }
  [[nodiscard]] const std::optional<WorkspaceSnapshot> &snapshot() const noexcept
  {
    return m_snapshot;
  }

  // Intents. `revision` must equal the displayed revision; a mismatch, an
  // unknown id, a missing grant, or a pending operation refuses with feedback
  // before any dispatch. Switching to the current desktop is a no-op success.
  Q_INVOKABLE bool switchTo(const QString &desktopId, quint64 revision);
  // Wraps around the sorted list; refused under the same rules as switchTo.
  Q_INVOKABLE bool switchRelative(int delta);
  Q_INVOKABLE bool setShowingDesktop(bool showing);
  Q_INVOKABLE bool toggleShowingDesktop();
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void stateChanged();
  void feedbackChanged();

private:
  enum class PendingKind { Switch, ShowDesktop };
  struct Pending {
    PendingKind kind = PendingKind::Switch;
    quint64 token = 0;
    QString owner;
  };

  void handleOwnerChanged(const QString &owner, const QString &reasonCode);
  void handleChanged(const QString &owner);
  void handleSnapshot(quint64 token, const QString &owner,
                      const WorkspaceSnapshot &snapshot);
  void handleSnapshotFailed(quint64 token, const QString &owner,
                            const QString &reasonCode);
  void handleOperationFinished(quint64 token, const QString &owner, bool ok,
                               const QString &reasonCode);
  void fetchSnapshot();
  void setPhase(WorkspacePhase phase, const QString &reason);
  bool refuse(const QString &message);
  void publishFeedback(const QString &message);
  [[nodiscard]] bool dispatchAllowed(QString *message) const;

  WorkspaceTransport *m_transport = nullptr;
  WorkspaceGrants m_grants;
  WorkspacePhase m_phase = WorkspacePhase::Loading;
  QString m_phaseReason;
  QString m_owner;
  std::optional<WorkspaceSnapshot> m_snapshot;
  quint64 m_revision = 0;
  quint64 m_nextToken = 1;
  quint64 m_fetchToken = 0;
  bool m_fetchDirty = false;
  std::optional<Pending> m_pending;
  QString m_feedback;
};

} // namespace QindaQt::Shell::Workspaces
