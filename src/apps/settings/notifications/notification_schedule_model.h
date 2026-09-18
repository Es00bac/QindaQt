// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Apps::SettingsNotifications {

// AGENT-CONTRACT: the Do Not Disturb schedule as the Notifications route sees
// it (ADR-0212). It owns no persistence: it reads the three
// `services.doNotDisturb*` keys from the injected Settings1 client's snapshot
// and writes through the same client, so what it publishes is always what the
// service last confirmed.
//
// AGENT-GUARD: the published values never move to what the user just asked
// for. A refused or uncommitted write leaves the controls where they are, so
// the page cannot claim the machine goes quiet at a time it does not.
//
// AGENT-GUARD: minutes are 0..1439 and a window whose ends are equal is empty,
// not all-day — the same reading NotificationInterruptionPolicy uses. Do not
// let these two definitions drift apart.
class NotificationScheduleModel final : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool available READ available NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool scheduleEnabled READ scheduleEnabled NOTIFY viewChanged FINAL)
  Q_PROPERTY(int startMinutes READ startMinutes NOTIFY viewChanged FINAL)
  Q_PROPERTY(int endMinutes READ endMinutes NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString startText READ startText NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString endText READ endText NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString summaryText READ summaryText NOTIFY viewChanged FINAL)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged FINAL)

public:
  static constexpr int minutesPerDay = 24 * 60;

  explicit NotificationScheduleModel(Services::SettingsClient::SettingsClient &client,
                                     QObject *parent = nullptr);

  Q_INVOKABLE void setScheduleEnabled(bool enabled);
  // Hours 0..23, minutes 0..59; anything else is refused whole.
  Q_INVOKABLE void setStart(int hour, int minute);
  Q_INVOKABLE void setEnd(int hour, int minute);
  Q_INVOKABLE void clearError();

  [[nodiscard]] bool available() const { return m_available; }
  [[nodiscard]] bool scheduleEnabled() const { return m_enabled; }
  [[nodiscard]] int startMinutes() const { return m_startMinutes; }
  [[nodiscard]] int endMinutes() const { return m_endMinutes; }
  [[nodiscard]] QString startText() const;
  [[nodiscard]] QString endText() const;
  [[nodiscard]] QString summaryText() const;
  [[nodiscard]] QString errorText() const { return m_errorText; }

  // "22:00" for 1320. Shared with the tests so the format cannot drift.
  [[nodiscard]] static QString formatMinutes(int minutes);
  [[nodiscard]] static bool isMinuteOfDay(int minutes) noexcept;

signals:
  void viewChanged();

private:
  void applySnapshot();
  void write(const QString &key, const QVariant &value);

  Services::SettingsClient::SettingsClient &m_client;
  bool m_available = false;
  bool m_enabled = false;
  int m_startMinutes = 22 * 60;
  int m_endMinutes = 7 * 60;
  QString m_errorText;
};

} // namespace QindaQt::Apps::SettingsNotifications
