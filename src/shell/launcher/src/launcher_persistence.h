// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_pinned_recent.h"

#include <QObject>
#include <QString>
#include <QStringList>

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

// Pinned/recent persistence behind the public Settings1 client, under the
// documented key set shell.launcher.pinned / shell.launcher.recent. Values
// are bounded string lists of desktop-entry ids and nothing else.
//
// AGENT-CONTRACT: The borrowed client must outlive this controller, be scoped
// to exactly the two launcher keys, and be started/stopped by the composition
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
  static QString pinnedKey() { return QStringLiteral("shell.launcher.pinned"); }
  static QString recentKey() { return QStringLiteral("shell.launcher.recent"); }

  explicit LauncherPersistenceController(
      QindaQt::Services::SettingsClient::SettingsClient &client, QObject *parent = nullptr);

  [[nodiscard]] const PinnedApplications &pinned() const noexcept { return m_pinned; }
  [[nodiscard]] const RecentApplications &recent() const noexcept { return m_recent; }
  [[nodiscard]] bool persistenceReady() const noexcept;
  [[nodiscard]] bool writeInFlight() const noexcept { return !m_pendingKey.isEmpty(); }
  [[nodiscard]] QString statusText() const { return m_statusText; }

  PersistenceMutation pin(const QString &entryId);
  PersistenceMutation unpin(const QString &entryId);
  PersistenceMutation movePinnedUp(const QString &entryId);
  PersistenceMutation movePinnedDown(const QString &entryId);
  PersistenceMutation clearRecent();
  // Recording a launch is best-effort within the same persistence rules:
  // Unavailable/Busy simply skip the record instead of queuing unboundedly.
  PersistenceMutation recordLaunch(const QString &entryId);

Q_SIGNALS:
  void pinnedChanged();
  void recentChanged();
  void stateChanged();

private:
  void handleSnapshot();
  void handleCommit(
      const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
  void handleUncertain(const QString &message);
  void handleClientState();
  void clearAuthoritativeTruth();
  PersistenceMutation mutatePinned(const QString &key,
                                   QindaQt::ShellLauncher::PinError (PinnedApplications::*op)(const QString &),
                                   const QString &entryId);
  PersistenceMutation commitList(const QString &key, const QStringList &ids);
  void revertToConfirmed(const QString &key);
  void setStatusText(const QString &text);

  QindaQt::Services::SettingsClient::SettingsClient &m_client;
  PinnedApplications m_pinned;
  RecentApplications m_recent;
  QStringList m_confirmedPinned;
  QStringList m_confirmedRecent;
  QString m_pendingKey;
  QString m_statusText;
  bool m_confirmedBaseline = false;
};

// Validates one stored id list: every element must be a valid, unique
// desktop-entry id and the complete list must fit the model ceiling. Returns
// false for any shape, element, duplicate, or size violation; the caller then
// treats the whole key as absent rather than partially trusting it.
[[nodiscard]] bool normalizeStoredIdList(const QVariant &stored, int ceiling,
                                         QStringList *ids);

} // namespace QindaQt::Shell::Launcher
