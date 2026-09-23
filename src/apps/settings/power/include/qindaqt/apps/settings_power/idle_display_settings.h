// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/session/desktop_controls/settings1_idle_preferences.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QObject>
#include <QString>
#include <QTimer>

namespace QindaQt::Apps::SettingsPower {

// AGENT-CONTRACT: the resident preferences provider may use its documented
// fallback while Settings1 is absent. This route instead presents only a
// current-owner accepted snapshot as editable policy. The borrowed provider
// and client outlive this same-thread QObject; no uncertain write is replayed.
class IdleDisplaySettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool hasConfirmed READ hasConfirmed NOTIFY changed)
  Q_PROPERTY(bool available READ available NOTIFY changed)
  Q_PROPERTY(bool canEdit READ canEdit NOTIFY changed)
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(int minutes READ minutes NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool conflict READ conflict NOTIFY changed)
  Q_PROPERTY(bool uncertain READ uncertain NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

public:
  explicit IdleDisplaySettingsModel(
      Session::DesktopControls::Settings1IdlePreferences &preferences,
      Services::SettingsClient::SettingsClient &client, QObject *parent = nullptr);
  ~IdleDisplaySettingsModel() override;

  IdleDisplaySettingsModel(const IdleDisplaySettingsModel &) = delete;
  IdleDisplaySettingsModel &operator=(const IdleDisplaySettingsModel &) = delete;

  [[nodiscard]] bool hasConfirmed() const noexcept { return m_hasConfirmed; }
  [[nodiscard]] bool available() const noexcept { return m_available; }
  [[nodiscard]] bool canEdit() const;
  [[nodiscard]] bool enabled() const noexcept;
  [[nodiscard]] int minutes() const noexcept;
  [[nodiscard]] bool busy() const noexcept { return m_pending; }
  [[nodiscard]] bool conflict() const noexcept { return m_conflict; }
  [[nodiscard]] bool uncertain() const noexcept { return m_uncertain; }
  [[nodiscard]] const QString &statusText() const noexcept { return m_statusText; }
  [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }

  Q_INVOKABLE bool setEnabled(bool enabled);
  Q_INVOKABLE bool setMinutes(int minutes);
  // Retry reads authority only. It never repeats the last requested value.
  Q_INVOKABLE bool retry();

Q_SIGNALS:
  void changed();

private:
  void applySnapshot(bool fresh);
  void handleClientState();
  void handleCommit(const Services::SettingsClient::CommitOutcome &outcome);
  void markUncertain(const QString &reason);
  void publishStatus();
  [[nodiscard]] bool submit(qint64 persistedMinutes);

  Session::DesktopControls::Settings1IdlePreferences &m_preferences;
  Services::SettingsClient::SettingsClient &m_client;
  QTimer m_readbackTimeout;
  QString m_statusText;
  QString m_errorText;
  QString m_writeOwner;
  QString m_writeEpoch;
  QString m_confirmedOwner;
  QString m_confirmedEpoch;
  qint64 m_confirmedMinutes = 0;
  qint64 m_requestedMinutes = 0;
  quint64 m_revisionFloor = 0;
  quint64 m_snapshotSequence = 0;
  quint64 m_snapshotSequenceAtDispatch = 0;
  quint64 m_requiredSnapshotSequence = 0;
  int m_lastPositiveMinutes = 10;
  bool m_hasConfirmed = false;
  bool m_available = false;
  bool m_pending = false;
  bool m_waitingForReadback = false;
  bool m_conflict = false;
  bool m_uncertain = false;
};

} // namespace QindaQt::Apps::SettingsPower
