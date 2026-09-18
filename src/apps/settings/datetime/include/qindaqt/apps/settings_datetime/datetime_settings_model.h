// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "system_time_service.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::SettingsDateTime {

// AGENT-CONTRACT: the same-thread projection the Date & time route binds to
// (ADR-0200). It owns the injected SystemTimeService and WeekStartPreference
// for its whole lifetime, publishes one complete view, and turns a user
// intent into exactly one platform request. It never reads the environment,
// never runs a helper, never sets the wall clock, and never reports success
// before the platform confirms it.
//
// AGENT-GUARD: the published values are always what the platform last said,
// never what the user just asked for. A control that moved optimistically
// would tell the user their timezone changed when polkit refused.
class DateTimeSettingsModel final : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool available READ available NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString timeZone READ timeZone NOTIFY viewChanged FINAL)
  Q_PROPERTY(QStringList timeZones READ timeZones NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool timeZoneEditable READ timeZoneEditable NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool automaticTime READ automaticTime NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool automaticTimeSupported READ automaticTimeSupported NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString synchronizationText READ synchronizationText NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString locale READ locale NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString weekStart READ weekStart NOTIFY viewChanged FINAL)
  Q_PROPERTY(QStringList weekStarts READ weekStarts CONSTANT FINAL)
  Q_PROPERTY(bool weekStartEditable READ weekStartEditable NOTIFY viewChanged FINAL)
  // Set while a platform request is outstanding. Controls stay enabled --
  // disabling them would make a slow authentication prompt look like a hang.
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged FINAL)

public:
  DateTimeSettingsModel(SystemTimeServicePtr service,
                        WeekStartPreferencePtr weekStart,
                        QObject *parent = nullptr);

  Q_INVOKABLE void requestTimeZone(const QString &timeZone);
  Q_INVOKABLE void requestAutomaticTime(bool enabled);
  Q_INVOKABLE void requestWeekStart(const QString &weekStart);
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void clearError();

  [[nodiscard]] bool available() const { return m_snapshot.available; }
  [[nodiscard]] QString timeZone() const { return m_snapshot.timeZone; }
  [[nodiscard]] QStringList timeZones() const { return m_snapshot.timeZones; }
  [[nodiscard]] bool timeZoneEditable() const;
  [[nodiscard]] bool automaticTime() const { return m_snapshot.automaticTime; }
  [[nodiscard]] bool automaticTimeSupported() const {
    return m_snapshot.available && m_snapshot.automaticTimeSupported;
  }
  [[nodiscard]] QString synchronizationText() const;
  [[nodiscard]] QString locale() const { return m_snapshot.locale; }
  [[nodiscard]] QString weekStart() const;
  [[nodiscard]] QStringList weekStarts() const;
  [[nodiscard]] bool weekStartEditable() const;
  [[nodiscard]] bool busy() const { return m_pendingRequests > 0; }
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] QString errorText() const { return m_errorText; }

  // Test seam independent of QML marshalling.
  [[nodiscard]] SystemTimeSnapshot snapshot() const { return m_snapshot; }

signals:
  void viewChanged();

private:
  void onSnapshotChanged(const SystemTimeSnapshot &snapshot);
  void onRequestFailed(const QString &diagnostic);
  void finishRequest();

  SystemTimeServicePtr m_service;
  WeekStartPreferencePtr m_weekStart;
  SystemTimeSnapshot m_snapshot;
  int m_pendingRequests = 0;
  QString m_errorText;
};

} // namespace QindaQt::Apps::SettingsDateTime
