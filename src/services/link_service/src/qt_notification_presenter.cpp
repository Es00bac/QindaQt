// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/qt_notification_presenter.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Link {
namespace {
constexpr auto kService = "org.freedesktop.Notifications";
constexpr auto kPath = "/org/freedesktop/Notifications";
}

QtNotificationPresenter::QtNotificationPresenter(QDBusConnection bus, QObject *parent)
    : PairingPresenter(parent), m_bus(std::move(bus))
{
    m_bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kService),
                  QStringLiteral("ActionInvoked"), this, SLOT(onActionInvoked(uint, QString)));
    m_bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kService),
                  QStringLiteral("NotificationClosed"), this, SLOT(onClosed(uint, uint)));
}

void QtNotificationPresenter::present(const QString &deviceId, const QString &name, const QString &model,
                                      const QString &code)
{
    QDBusMessage m = QDBusMessage::createMethodCall(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                                                    QString::fromLatin1(kService), QStringLiteral("Notify"));
    const uint replaces = m_byDevice.value(deviceId, 0);
    m << QStringLiteral("QindaLink") << replaces << QStringLiteral("phone")
      << QStringLiteral("Pair with %1?").arg(name)
      << QStringLiteral("%1 (%2) shows the same code: %3").arg(name, model, spacedCode(code))
      << QStringList{QStringLiteral("accept"), QStringLiteral("Accept"), QStringLiteral("reject"),
                     QStringLiteral("Reject")}
      << QVariantMap{{QStringLiteral("resident"), true}} << 0;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(m), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, deviceId](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<uint> reply = *w;
        if (reply.isValid()) {
            m_byNotification.insert(reply.value(), deviceId);
            m_byDevice.insert(deviceId, reply.value());
        }
        w->deleteLater();
    });
}

void QtNotificationPresenter::dismiss(const QString &deviceId)
{
    const uint id = m_byDevice.take(deviceId);
    if (id == 0) {
        return;
    }
    m_byNotification.remove(id);
    QDBusMessage m = QDBusMessage::createMethodCall(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                                                    QString::fromLatin1(kService), QStringLiteral("CloseNotification"));
    m << id;
    m_bus.asyncCall(m);
}

void QtNotificationPresenter::onActionInvoked(uint id, const QString &key)
{
    const QString device = m_byNotification.take(id);
    if (device.isEmpty()) {
        return;
    }
    m_byDevice.remove(device);
    Q_EMIT decided(device, key == QLatin1String("accept"));
}

void QtNotificationPresenter::onClosed(uint id, uint)
{
    // Closing without choosing leaves the request pending in the app.
    const QString device = m_byNotification.take(id);
    m_byDevice.remove(device);
}

} // namespace QindaQt::Link
