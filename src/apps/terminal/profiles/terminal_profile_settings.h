// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "profiles/terminal_profile.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace QindaQt::Services::SettingsClient {
struct CommitOutcome;
class SettingsClient;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Apps::Terminal {

// Documented Settings1 keys (see docs/wiki/apps/terminal.md and
// data/settings/schema-v2.json). The scopedKeys() order is the fixed
// optimistic-commit order; it is also the client's exact snapshot scope.
namespace TerminalKeys {
inline constexpr QLatin1String Profiles{
    QLatin1String("services.terminalProfiles")};
inline constexpr QLatin1String DefaultProfile{
    QLatin1String("services.terminalDefaultProfile")};
inline constexpr QLatin1String RestoreWindows{
    QLatin1String("services.terminalRestoreWindows")};
[[nodiscard]] QStringList scopedKeys();
} // namespace TerminalKeys

// AGENT-CONTRACT: TerminalProfileSettings is the only consumer of the
// terminal Settings1 scope. It mirrors the ADR-0028 appearance truth in
// one-key form: confirmed state comes only from valid snapshots; a draft
// applies as single-key optimistic commits in fixed key order, one write
// in flight, never carrying a stale base revision; a conflict whose
// authoritative value already matches the draft counts as applied, any
// other conflict or confirmed rejection aborts the sequence; an uncertain
// outcome (timeout/owner loss/bus loss) is never replayed. Before the
// first baseline and after unrecoverable transport loss the built-in
// defaults apply (fail-closed); old confirmed values are never presented as
// current authority during loss/replacement. Profile values are the only
// persisted content — never session bytes or scrollback.
class TerminalProfileSettings final : public QObject {
  Q_OBJECT

public:
  explicit TerminalProfileSettings(
      QindaQt::Services::SettingsClient::SettingsClient &client,
      QObject *parent = nullptr);
  ~TerminalProfileSettings() override;

  // Confirmed state from the last valid snapshot (built-in defaults before
  // any baseline). Transport loss invalidates that authority immediately,
  // so these accessors return built-in defaults until a new baseline lands.
  [[nodiscard]] QList<TerminalProfile> userProfiles() const {
    return m_userProfiles;
  }
  [[nodiscard]] QString defaultProfileId() const { return m_defaultProfileId; }
  [[nodiscard]] bool restoreWindowsPolicy() const { return m_restoreWindows; }
  [[nodiscard]] bool baselineReceived() const { return m_baselineReceived; }
  // The default profile: the confirmed user default when set, otherwise
  // the built-in default. Always valid.
  [[nodiscard]] TerminalProfile defaultProfile() const;

  // Queues the draft as a fixed-order sequence of single-key commits.
  // Returns false (and changes nothing) while a sequence is already in
  // flight or the client is not Ready. The ledger signal reports one
  // bounded entry per key: "applied" | "failed" | "conflict" |
  // "uncertain" | "not-attempted".
  [[nodiscard]] bool applyProfiles(const QList<TerminalProfile> &profiles,
                                   const QString &defaultProfileId,
                                   bool restoreWindows);

signals:
  // Confirmed state changed from a snapshot. During an apply this also
  // drives the next queued commit once the post-commit refresh lands.
  void profilesChanged();
  // Emitted exactly once per applyProfiles() call after the sequence
  // settles (including aborts). Each entry: {key, result, message}.
  void applyFinished(const QVariantList &ledger);

private:
  struct QueuedWrite final {
    QString key;
    QVariant value;
  };

  void handleSnapshot();
  void handleClientState();
  void handleCommitFinished(
      const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
  void handleCommitUncertain(const QString &message);
  void writeNextQueued();
  void finalizeLedger();
  void abortLedger(const QString &key, const QString &result,
                   const QString &message);
  void resetToBuiltins();

  QindaQt::Services::SettingsClient::SettingsClient &m_client;

  QList<TerminalProfile> m_userProfiles;
  QString m_defaultProfileId;
  bool m_restoreWindows = false;
  bool m_baselineReceived = false;
  QString m_confirmedOwner;
  QString m_confirmedEpoch;

  QList<QueuedWrite> m_queue;
  QString m_pendingKey;
  QVariantMap m_intendedValues;
  QVariantMap m_ledgerEntries; // key -> {result, message}
  bool m_ledgerActive = false;
  bool m_waitingForSnapshot = false;
  bool m_sequenceAborted = false;
};

} // namespace QindaQt::Apps::Terminal
