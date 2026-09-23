// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_datetime/system_time_service.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QDBusConnection>
#include <QTimer>

#include <memory>

namespace QindaQt::Services::SettingsClient {
class SettingsTransport;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Apps::SettingsDateTime {

// Settings1 adapter for the single calendarWeekStart key. The transport is
// owned by this object; an injected transport permits deterministic private
// tests. All callbacks run on the constructing QObject thread.
class SettingsWeekStartPreference final : public WeekStartPreference {
  Q_OBJECT

public:
  explicit SettingsWeekStartPreference(QDBusConnection bus,
                                       QObject *parent = nullptr);
  SettingsWeekStartPreference(
      std::unique_ptr<QindaQt::Services::SettingsClient::SettingsTransport> transport,
      QindaQt::Services::SettingsClient::ClientTiming timing = {},
      QObject *parent = nullptr);
  ~SettingsWeekStartPreference() override;

  [[nodiscard]] QString weekStart() const override;
  [[nodiscard]] bool editable() const override;
  [[nodiscard]] WeekStartWriteState writeState() const override;
  [[nodiscard]] QString diagnostic() const override;
  [[nodiscard]] QString availabilityText() const override;
  bool setWeekStart(const QString &weekStart) override;
  void refresh() override;

private:
  void applySnapshot();
  void handleClientState();
  void handleCommit(const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
  void markUncertain(const QString &message);

  std::unique_ptr<QindaQt::Services::SettingsClient::SettingsTransport> m_transport;
  std::unique_ptr<QindaQt::Services::SettingsClient::SettingsClient> m_client;
  QTimer m_readbackTimeout;
  QString m_weekStart = QStringLiteral("locale");
  QString m_requested;
  QString m_writeOwner;
  QString m_writeEpoch;
  QString m_diagnostic;
  quint64 m_revisionFloor = 0;
  WeekStartWriteState m_writeState = WeekStartWriteState::Idle;
  bool m_awaitingReadback = false;
};

} // namespace QindaQt::Apps::SettingsDateTime
