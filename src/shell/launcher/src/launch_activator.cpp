// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launch_activator.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

namespace QindaQt::Shell::Launcher {
namespace {

bool isValidDesktopId(const QString &desktopId)
{
  // A D-Bus well-known name: dot-separated non-empty elements of ASCII
  // letters, digits, underscore, hyphen; must contain a dot.
  if (desktopId.isEmpty() || !desktopId.contains(QLatin1Char('.')))
    return false;
  const QStringList elements = desktopId.split(QLatin1Char('.'));
  for (const QString &element : elements) {
    if (element.isEmpty())
      return false;
    for (const QChar character : element) {
      const ushort value = character.unicode();
      const bool letter = (value >= 'A' && value <= 'Z')
          || (value >= 'a' && value <= 'z');
      const bool digit = value >= '0' && value <= '9';
      if (!letter && !digit && value != '_' && value != '-')
        return false;
    }
  }
  return true;
}

QString objectPathFor(const QString &desktopId)
{
  QString path = desktopId;
  path.replace(QLatin1Char('.'), QLatin1Char('/'));
  path.replace(QLatin1Char('-'), QLatin1Char('_'));
  return QLatin1Char('/') + path;
}

} // namespace

SessionBusActivator::SessionBusActivator(const QDBusConnection &connection,
                                         QObject *parent)
    : LaunchActivator(parent), m_connection(connection)
{
}

ActivationDispatch SessionBusActivator::activate(const QString &desktopId,
                                                 const QString &actionId)
{
  if (!isValidDesktopId(desktopId)) {
    return { false, QStringLiteral("desktop id is not a valid D-Bus name") };
  }
  if (!m_connection.isConnected()) {
    return { false, QStringLiteral("session bus is not connected") };
  }

  // org.freedesktop.Application: Activate(platform_data) for the primary
  // action, ActivateAction(action, parameter, platform_data) otherwise. No
  // startup-notification token exists yet; the platform-data map is empty.
  const QVariantMap platformData;
  QDBusMessage message;
  if (actionId.isEmpty()) {
    message = QDBusMessage::createMethodCall(
        desktopId, objectPathFor(desktopId),
        QStringLiteral("org.freedesktop.Application"), QStringLiteral("Activate"));
    message.setArguments({ QVariant::fromValue(platformData) });
  } else {
    message = QDBusMessage::createMethodCall(
        desktopId, objectPathFor(desktopId),
        QStringLiteral("org.freedesktop.Application"),
        QStringLiteral("ActivateAction"));
    message.setArguments({ actionId, QVariantList(),
                           QVariant::fromValue(platformData) });
  }

  const QDBusPendingCall pending = m_connection.asyncCall(message);
  if (pending.isError()) {
    return { false, pending.error().message() };
  }
  auto *watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, desktopId](QDBusPendingCallWatcher *done) {
            const QDBusPendingReply<> reply = *done;
            if (reply.isError()) {
              Q_EMIT activationFinished(desktopId, false,
                                        reply.error().message());
            } else {
              Q_EMIT activationFinished(desktopId, true, {});
            }
            done->deleteLater();
          });
  return { true, {} };
}

} // namespace QindaQt::Shell::Launcher
