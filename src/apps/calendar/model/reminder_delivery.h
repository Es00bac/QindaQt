// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QDateTime>
#include <QObject>
#include <QString>

namespace QindaQt::Apps::Calendar {

// Posts due reminders to the freedesktop notification server over the
// injected D-Bus connection. When the bus or the notification service is
// unavailable, deliver() returns false and the caller falls back to the
// in-window reminder banner. The delivery path never qWarnings: offscreen
// probes run without a session bus under QT_FATAL_WARNINGS=1.
class ReminderDelivery final : public QObject {
  Q_OBJECT

public:
  explicit ReminderDelivery(QDBusConnection connection,
                            QObject *parent = nullptr);

  [[nodiscard]] bool deliver(const QString &summary,
                             const QDateTime &occurrenceStart);

private:
  QDBusConnection m_connection;
};

} // namespace QindaQt::Apps::Calendar
