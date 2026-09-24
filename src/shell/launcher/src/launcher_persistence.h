// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_pinned_recent.h"

#include <qindaqt/services/dock_items/dock_items.h>

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Shell::Launcher {

using QindaQt::ShellLauncher::PinnedApplications;
using QindaQt::ShellLauncher::RecentApplications;

// Outcome of one persistence mutation request.
enum class PersistenceMutation {
  Applied,         // model accepted the change and a commit was dispatched
  RejectedByModel, // pin/unpin/move/clear violated the bounded model rules
  Unavailable,     // no confirmed Settings1 baseline; fail closed
  Busy,            // a write is still in flight; the client serializes writes
};

// Pinned/recent persistence behind the public Settings1 client. Recent is the
// bounded id list panels.launcherRecent. Pins are the applications of the
// structured dock value panels.dockItems (ADR-0265): the dock owns their
// order, groups, and the folder/file/Trash items around them, and pinned() is
// its projection. Until the first dock edit writes that value, the dock is
// derived from ADR-0076's id list panels.launcherPinned (the one-time
// migration); that key is never written again.
//
// AGENT-CONTRACT: The borrowed client must outlive this controller, be scoped
// to a scope containing both launcher keys, and be started/stopped by the composition
// root. Semantics follow ADR-0012: a mutation applies to the live model and
// commits immediately; a confirmed rejection reverts the model to the last
// confirmed value and stays visible until the next explicit write; an
// uncertain commit is never replayed and resolves through the resync snapshot;
// owner or transport loss clears prior identity truth and refuses new writes.
class LauncherPersistenceController final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool persistenceReady READ persistenceReady NOTIFY stateChanged)
  Q_PROPERTY(bool writeInFlight READ writeInFlight NOTIFY stateChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

public:
  static QString pinnedKey() { return QStringLiteral("panels.launcherPinned"); }
  static QString recentKey() { return QStringLiteral("panels.launcherRecent"); }
  static QString dockItemsKey()
  {
    return QString::fromLatin1(QindaQt::Services::DockItems::DockItemsSettingsKey);
  }
  // AGENT-CONTRACT: the borrowed client's scope must contain all of these
  // (it may contain other keys). A client missing dockItemsKey() still reads
  // the migrated pins but refuses every dock write.
  static QStringList scopedKeys() { return {pinnedKey(), recentKey(), dockItemsKey()}; }

  using DockEdit = std::function<QindaQt::Services::DockItems::DockEditError(
      QindaQt::Services::DockItems::DockItems &)>;

  explicit LauncherPersistenceController(
      QindaQt::Services::SettingsClient::SettingsClient &client, QObject *parent = nullptr);

  // Every dock application in dock order (a projection of dock()).
  [[nodiscard]] const PinnedApplications &pinned() const noexcept { return m_pinned; }
  [[nodiscard]] const QindaQt::Services::DockItems::DockItems &dock() const noexcept
  {
    return m_dock;
  }
  // Why the last RejectedByModel dock edit was refused.
  [[nodiscard]] QindaQt::Services::DockItems::DockEditError lastDockEditError() const noexcept
  {
    return m_lastDockEditError;
  }
  [[nodiscard]] const RecentApplications &recent() const noexcept { return m_recent; }
  [[nodiscard]] bool persistenceReady() const noexcept;
  [[nodiscard]] bool writeInFlight() const noexcept { return !m_pendingKey.isEmpty(); }
  [[nodiscard]] QString statusText() const { return m_statusText; }

  PersistenceMutation pin(const QString &entryId);
  PersistenceMutation unpin(const QString &entryId);
  PersistenceMutation movePinnedUp(const QString &entryId);
  PersistenceMutation movePinnedDown(const QString &entryId);
  // Applies one edit to a copy of the live dock and, when the edit succeeds,
  // publishes and commits the whole dock value under the rules above. The pin
  // operations are edits of the same kind; move up/down move the top-level
  // item holding the application.
  PersistenceMutation editDock(const DockEdit &edit);
  PersistenceMutation clearRecent();
  // Recording a launch is best-effort within the same persistence rules:
  // Unavailable/Busy simply skip the record instead of queuing unboundedly.
  PersistenceMutation recordLaunch(const QString &entryId);

Q_SIGNALS:
  void pinnedChanged();
  // Any change of the live dock, including ones pinned() does not show
  // (folders, files, Trash, group names and membership order).
  void dockChanged();
  void recentChanged();
  void stateChanged();

private:
  void handleSnapshot();
  void handleCommit(
      const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
  void handleUncertain(const QString &message);
  void handleClientState();
  void clearAuthoritativeTruth();
  void adoptDock(const QindaQt::Services::DockItems::DockItems &dock);
  PersistenceMutation commitValue(const QString &key, const QVariant &value);
  void revertToConfirmed(const QString &key);
  void setStatusText(const QString &text, bool availabilityNotice = false);

  QindaQt::Services::SettingsClient::SettingsClient &m_client;
  QindaQt::Services::DockItems::DockItems m_dock;
  QindaQt::Services::DockItems::DockItems m_confirmedDock;
  PinnedApplications m_pinned;
  RecentApplications m_recent;
  QStringList m_confirmedRecent;
  QindaQt::Services::DockItems::DockEditError m_lastDockEditError =
      QindaQt::Services::DockItems::DockEditError::None;
  QString m_pendingKey;
  QString m_statusText;
  bool m_availabilityNotice = false;
  bool m_confirmedBaseline = false;
};

// Validates one stored id list (the recent list; the legacy pinned list goes
// through DockItems::fromLegacyValue): every element must be a valid, unique
// desktop-entry id and the complete list must fit the model ceiling. Returns
// false for any shape, element, duplicate, or size violation; the caller then
// treats the whole key as absent rather than partially trusting it.
[[nodiscard]] bool normalizeStoredIdList(const QVariant &stored, int ceiling,
                                         QStringList *ids);

} // namespace QindaQt::Shell::Launcher
