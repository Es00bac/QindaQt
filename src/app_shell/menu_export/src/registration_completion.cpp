// SPDX-License-Identifier: LGPL-3.0-or-later
#include "registration_completion_p.h"

#include <QDBusError>
#include <QDBusMessage>

namespace QindaQt::AppShell::MenuExport {

RegistrationCompletion classifyRegistrationCompletion(const QDBusMessage &reply) {
  if (reply.type() == QDBusMessage::ReplyMessage) {
    return reply.signature().isEmpty() ? RegistrationCompletion::Confirmed
                                       : RegistrationCompletion::Uncertain;
  }
  if (reply.type() != QDBusMessage::ErrorMessage) {
    return RegistrationCompletion::Uncertain;
  }
  const QDBusError error(reply);
  if (error.name() == QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner")) {
    return RegistrationCompletion::Uncertain;
  }
  switch (error.type()) {
  case QDBusError::ServiceUnknown:
  case QDBusError::NoReply:
  case QDBusError::BadAddress:
  case QDBusError::NoServer:
  case QDBusError::Timeout:
  case QDBusError::NoNetwork:
  case QDBusError::Disconnected:
  case QDBusError::TimedOut:
    return RegistrationCompletion::Uncertain;
  default:
    return RegistrationCompletion::Refused;
  }
}

} // namespace QindaQt::AppShell::MenuExport
