// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationliveresident.h"

#include "notificationliveruntime.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <QVariantMap>

namespace QindaQt::Test {
namespace {

constexpr auto OwnerObjectPath = "/org/qindaqt/NotificationLiveOwner";
constexpr auto OwnerInterface = "org.qindaqt.NotificationLiveOwner1";

std::optional<quint32> submitResidentNotification(QString *error)
{
    QDBusInterface notifications(
        QString::fromLatin1(NotificationLiveNotificationService),
        QStringLiteral("/org/freedesktop/Notifications"),
        QString::fromLatin1(NotificationLiveNotificationService));
    const QVariantMap hints{
        {QStringLiteral("urgency"), QVariant::fromValue(static_cast<uchar>(1))}};
    const QDBusReply<quint32> reply = notifications.call(
        QStringLiteral("Notify"), QStringLiteral("QindaQt live qualification"),
        quint32(0), QString{}, QStringLiteral("Resident across shell restart"),
        QStringLiteral("Fresh baseline and no-replay qualification"),
        QStringList{}, hints, 0);
    if (!reply.isValid() || reply.value() == 0) {
        *error = QStringLiteral("resident notification submission failed: %1")
                     .arg(reply.error().message());
        return std::nullopt;
    }
    return reply.value();
}

class ResidentNotificationOwner final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.NotificationLiveOwner1")

public:
    explicit ResidentNotificationOwner(quint32 notificationId)
        : m_notificationId(notificationId)
    {}

public Q_SLOTS:
    bool CloseNotification(quint32 id)
    {
        if (m_closed || id == 0 || id != m_notificationId) {
            return false;
        }
        QDBusInterface notifications(
            QString::fromLatin1(NotificationLiveNotificationService),
            QStringLiteral("/org/freedesktop/Notifications"),
            QString::fromLatin1(NotificationLiveNotificationService));
        const QDBusMessage reply =
            notifications.call(QStringLiteral("CloseNotification"), id);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            return false;
        }
        m_closed = true;
        // Return the method reply before dropping the unique sender that owns
        // the notification. Python still reaps this exact private child.
        QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
        return true;
    }

private:
    quint32 m_notificationId = 0;
    bool m_closed = false;
};

} // namespace

int runResidentNotificationOwner()
{
    QString error;
    const auto notificationId = submitResidentNotification(&error);
    if (!notificationId) {
        QTextStream(stderr) << error << '\n';
        return 1;
    }
    ResidentNotificationOwner owner(*notificationId);
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(NotificationLiveResidentOwnerService))
        || !bus.registerObject(QString::fromLatin1(OwnerObjectPath), &owner,
                               QDBusConnection::ExportAllSlots)) {
        QTextStream(stderr) << "could not publish private resident owner\n";
        return 1;
    }
    QTextStream output(stdout);
    output << "QINDAQT_NOTIFICATION_RESIDENT="
           << QJsonDocument(QJsonObject{{QStringLiteral("notificationId"),
                                        QString::number(*notificationId)}})
                  .toJson(QJsonDocument::Compact)
           << '\n';
    output.flush();
    return QCoreApplication::instance()->exec();
}

bool closeResidentNotification(quint32 id, qint64 expectedOwnerProcessId, QString *error)
{
    auto *const busInterface = QDBusConnection::sessionBus().interface();
    if (busInterface == nullptr || expectedOwnerProcessId <= 1) {
        *error = QStringLiteral("resident notification owner expectation is invalid");
        return false;
    }
    const QDBusReply<uint> ownerPid = busInterface->servicePid(
        QString::fromLatin1(NotificationLiveResidentOwnerService));
    if (!ownerPid.isValid()
        || static_cast<qint64>(ownerPid.value()) != expectedOwnerProcessId) {
        *error = QStringLiteral("resident notification owner PID mismatch: expected=%1 observed=%2")
                     .arg(expectedOwnerProcessId)
                     .arg(ownerPid.isValid() ? QString::number(ownerPid.value())
                                             : ownerPid.error().message());
        return false;
    }
    QDBusInterface owner(QString::fromLatin1(NotificationLiveResidentOwnerService),
                         QString::fromLatin1(OwnerObjectPath),
                         QString::fromLatin1(OwnerInterface));
    const QDBusReply<bool> reply = owner.call(QStringLiteral("CloseNotification"), id);
    if (!reply.isValid() || !reply.value()) {
        *error = QStringLiteral("resident notification owner failed to close id %1: %2")
                     .arg(id)
                     .arg(reply.error().message());
        return false;
    }
    return true;
}

} // namespace QindaQt::Test

#include "notificationliveresident.moc"
