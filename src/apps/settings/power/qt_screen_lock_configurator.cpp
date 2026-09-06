// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_power/screen_lock_settings.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Apps::SettingsPower {
namespace {
constexpr auto Service = "org.kde.screensaver";
constexpr auto Path = "/ScreenSaver";
constexpr auto Interface = "org.kde.screensaver";
} // namespace

QtScreenLockConfigureClient::QtScreenLockConfigureClient(QObject *parent)
    : ScreenLockConfigureClient(parent) {}

void QtScreenLockConfigureClient::requestConfigure() {
  const QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(Service), QString::fromLatin1(Path),
      QString::fromLatin1(Interface), QStringLiteral("configure"));
  auto *watcher = new QDBusPendingCallWatcher(
      QDBusConnection::sessionBus().asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
    const QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();
    Q_EMIT configured(!reply.isError(), reply.isError() ? reply.error().message() : QString{});
  });
}

} // namespace QindaQt::Apps::SettingsPower
