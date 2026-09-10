// SPDX-License-Identifier: GPL-3.0-or-later
#include "reminder_delivery.h"

#include <QDBusMessage>
#include <QLocale>
#include <QVariantMap>

namespace QindaQt::Apps::Calendar {

ReminderDelivery::ReminderDelivery(QDBusConnection connection, QObject *parent)
    : QObject(parent), m_connection(std::move(connection)) {}

bool ReminderDelivery::deliver(const QString &summary,
                               const QDateTime &occurrenceStart) {
  if (!m_connection.isConnected()) {
    return false;
  }
  const QString body =
      QLocale().toString(occurrenceStart, QLocale::ShortFormat);
  QDBusMessage message = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("Notify"));
  message.setArguments({
      QStringLiteral("QindaQt Calendar"), // app_name
      quint32(0),                          // replaces_id
      QStringLiteral("org.qindaqt.Calendar"), // app_icon
      summary,                             // summary
      body,                                // body
      QStringList{},                       // actions
      QVariantMap{},                       // hints
      -1,                                  // expire_timeout: server default
  });
  const QDBusMessage reply = m_connection.call(
      message, QDBus::BlockWithGui, 2'000);
  return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace QindaQt::Apps::Calendar
