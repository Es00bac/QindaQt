// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationshellreadiness.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QJsonDocument>

namespace QindaQt::Test {
namespace {

constexpr auto ServiceName = "org.qindaqt.ShellDevelopment";
constexpr auto ObjectPath = "/org/qindaqt/ShellDevelopment";
constexpr auto InterfaceName = "org.qindaqt.ShellDevelopment1";

} // namespace

DesktopNotificationShellCheck sampleDesktopNotificationShell(
    const QDBusConnection &connection, QDBusConnectionInterface &bus,
    const DesktopNotificationShellExpectation &expectation)
{
    DesktopNotificationShellObservation observation;
    const QDBusReply<QString> owner =
        bus.serviceOwner(QString::fromLatin1(ServiceName));
    if (!owner.isValid()) {
        observation.serviceOwnerReplyValid = false;
        observation.serviceOwnerReplyError = owner.error().message();
        observation.serviceOwnerReplyErrorName = owner.error().name();
        return validateDesktopNotificationShell(observation, expectation);
    }
    if (owner.value().isEmpty()) {
        return validateDesktopNotificationShell(observation, expectation);
    }
    observation.owner = owner.value();
    const QDBusReply<quint32> processId = bus.servicePid(observation.owner);
    if (processId.isValid()) {
        observation.serviceProcessId = static_cast<qint64>(processId.value());
    }
    // AGENT-GUARD: Call the sampled unique owner, then prove the well-known
    // name still resolves to it. Calling the replaceable name would splice
    // generations across PID authentication and Snapshot.
    const QDBusMessage request = QDBusMessage::createMethodCall(
        observation.owner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(InterfaceName), QStringLiteral("Snapshot"));
    const QDBusReply<QByteArray> reply(
        connection.call(request, QDBus::Block, 250));
    observation.snapshotReplyValid = reply.isValid();
    if (!reply.isValid()) {
        observation.replyError = reply.error().message();
        observation.replyErrorName = reply.error().name();
    } else {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(reply.value(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            observation.snapshot = document.object();
        }
    }
    const QDBusReply<QString> ownerAfter =
        bus.serviceOwner(QString::fromLatin1(ServiceName));
    if (ownerAfter.isValid()) {
        observation.ownerAfterSnapshot = ownerAfter.value();
    }
    return validateDesktopNotificationShell(observation, expectation);
}

} // namespace QindaQt::Test
