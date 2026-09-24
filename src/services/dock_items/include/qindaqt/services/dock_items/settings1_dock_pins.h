// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/dock_items/dock_items.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <optional>

namespace QindaQt::Services::DockItems {

// The documented way for a process other than the shell to pin or unpin one
// application in the dock (ADR-0265): for example the File Manager's
// Applications "Keep in Dock". It reads the confirmed dock (migrating the
// legacy pinned list exactly as the shell does), applies one edit through the
// shared DockItems codec, writes the whole structured value, and reports
// Saved only after a same-owner, same-epoch snapshot at or beyond the Applied
// revision reads back the requested dock.
//
// AGENT-CONTRACT: borrows a SettingsClient whose scope is exactly
// scopedKeys(); the owner starts and stops that client, keeps it alive longer
// than this object, and uses both on one thread. A request that returns true
// was admitted, not saved: observe requestFinished. Nothing is replayed after
// a refusal, a timeout, or an owner change. The shell's dock adopts the new
// value from the Settings1 change signal like any other writer's; this helper
// never talks to the shell.
class Settings1DockPins final : public QObject {
  Q_OBJECT

public:
  enum class Outcome {
    // A fresh snapshot read back the requested dock.
    Saved,
    // Settings1 refused the write; the dock is unchanged.
    Refused,
    // Another writer changed the dock first; check isPinned() again.
    Conflict,
    // The write may or may not have landed (timeout or service change).
    Uncertain,
  };
  Q_ENUM(Outcome)

  static const QStringList &scopedKeys();

  explicit Settings1DockPins(SettingsClient::SettingsClient &client,
                             QObject *parent = nullptr);

  // A confirmed dock is known and the current Settings1 owner vouches for it.
  [[nodiscard]] bool isLoaded() const;
  [[nodiscard]] bool writePending() const noexcept { return m_pending.has_value(); }
  // Human-readable progress or refusal text; diagnostic, not a protocol.
  [[nodiscard]] QString statusText() const { return m_status; }
  // Confirmed truth only: false while nothing is loaded.
  [[nodiscard]] bool isPinned(const QString &applicationId) const;

  // Appends the application to the end of the dock.
  bool pinApplication(const QString &applicationId);
  // Removes the application wherever it is, a group member included.
  bool unpinApplication(const QString &applicationId);

Q_SIGNALS:
  void pinsChanged();
  void statusChanged();
  void requestFinished(const QString &applicationId, bool pin,
                       QindaQt::Services::DockItems::Settings1DockPins::Outcome outcome);

private:
  struct Pending final {
    QString applicationId;
    bool pin = true;
    DockItems requested;
    QString owner;
    QString epoch;
    quint64 revisionFloor = 0;
    bool awaitingReadback = false;
  };

  bool request(const QString &applicationId, bool pin);
  void onSnapshotChanged();
  void onStateChanged();
  void onCommitFinished(const SettingsClient::CommitOutcome &outcome);
  void onCommitUncertain(const QString &message);
  void onReadbackTick();
  void finish(Outcome outcome, const QString &status);
  void setStatus(const QString &status);

  SettingsClient::SettingsClient &m_client;
  // The last confirmed dock; nothing while unloaded or when the stored value
  // is malformed (writes then refuse rather than overwrite unknown data).
  std::optional<DockItems> m_dock;
  bool m_loaded = false;
  std::optional<Pending> m_pending;
  QElapsedTimer m_readbackAge;
  QTimer m_readbackTimer;
  QString m_status;
};

} // namespace QindaQt::Services::DockItems
