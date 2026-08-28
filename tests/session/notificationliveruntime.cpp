// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationliveruntime.h"

#include "compositorprobeclient.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>

#include <cerrno>
#include <cmath>
#include <cstring>
#include <limits>
#include <sys/types.h>
#include <unistd.h>

namespace QindaQt::Test {

bool awaitNotificationLiveCondition(const std::function<bool()> &condition, int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (condition()) {
            return true;
        }
        QThread::msleep(20);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return condition();
}

std::optional<QString> notificationLiveServiceOwner(const QString &service, QString *error)
{
    auto *const interface = QDBusConnection::sessionBus().interface();
    if (interface == nullptr) {
        *error = QStringLiteral("private session bus has no daemon interface");
        return std::nullopt;
    }
    const QDBusReply<QString> reply = interface->serviceOwner(service);
    if (!reply.isValid() || reply.value().isEmpty()) {
        *error = QStringLiteral("%1 has no unique owner: %2").arg(service, reply.error().message());
        return std::nullopt;
    }
    return reply.value();
}

std::optional<qint64> notificationLiveServicePid(const QString &service, QString *error)
{
    auto *const interface = QDBusConnection::sessionBus().interface();
    if (interface == nullptr) {
        *error = QStringLiteral("private session bus has no daemon interface");
        return std::nullopt;
    }
    const QDBusReply<uint> reply = interface->servicePid(service);
    if (!reply.isValid() || reply.value() <= 1) {
        *error = QStringLiteral("%1 has no usable bus process id: %2")
                     .arg(service, reply.error().message());
        return std::nullopt;
    }
    return static_cast<qint64>(reply.value());
}

bool validateNotificationLiveSignalTarget(qint64 processId, QString *error)
{
    if (processId <= 1 || processId > static_cast<qint64>(std::numeric_limits<pid_t>::max())) {
        *error = QStringLiteral("refused invalid private signal PID %1").arg(processId);
        return false;
    }
    errno = 0;
    const pid_t targetSessionId = ::getsid(static_cast<pid_t>(processId));
    const int targetError = errno;
    errno = 0;
    const pid_t driverSessionId = ::getsid(0);
    const int driverError = errno;
    if (targetSessionId < 0 || driverSessionId < 0) {
        *error = QStringLiteral("could not authenticate private signal PID %1: target=%2 driver=%3")
                     .arg(processId)
                     .arg(QString::fromLocal8Bit(std::strerror(targetError)))
                     .arg(QString::fromLocal8Bit(std::strerror(driverError)));
        return false;
    }
    if (targetSessionId != driverSessionId) {
        *error = QStringLiteral("refused signal outside private session: pid=%1 "
                                "targetSid=%2 driverSid=%3")
                     .arg(processId)
                     .arg(targetSessionId)
                     .arg(driverSessionId);
        return false;
    }
    error->clear();
    return true;
}

bool awaitNotificationLiveService(const QString &service)
{
    return awaitNotificationLiveCondition([&] {
        QString ignored;
        return notificationLiveServiceOwner(service, &ignored).has_value();
    });
}

std::optional<QJsonObject>
validateNotificationLiveRuntime(const NotificationLiveExpectations &expectations, QString *error)
{
    if (qEnvironmentVariable("QINDAQT_NOTIFICATION_LIVE_PRIVATE_BUS") != QLatin1String("1")
        || qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS")
        || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        || !qEnvironmentVariableIsEmpty("DISPLAY")) {
        *error = QStringLiteral("probe refused a non-private bus or inherited host display");
        return std::nullopt;
    }
    if (!QDBusConnection::sessionBus().isConnected()
        || !awaitNotificationLiveService(QString::fromLatin1(NotificationLiveCompositorService))) {
        *error = QStringLiteral("nested compositor service did not appear");
        return std::nullopt;
    }

    QString pidError;
    const auto compositorPid = notificationLiveServicePid(
        QString::fromLatin1(NotificationLiveCompositorService), &pidError);
    if (!compositorPid || *compositorPid != expectations.compositorProcessId) {
        *error = QStringLiteral("compositor bus PID mismatch: %1").arg(pidError);
        return std::nullopt;
    }

    CompositorProbeClient compositor;
    const auto capabilities = compositor.call(QStringLiteral("Capabilities"), error);
    if (!capabilities) {
        return std::nullopt;
    }
    const QJsonObject developmentInput =
        capabilities->value(QStringLiteral("developmentInput")).toObject();
    if (!developmentInput.value(QStringLiteral("enabled")).toBool()
        || !developmentInput.value(QStringLiteral("available")).toBool()
        || developmentInput.value(QStringLiteral("deviceId"))
               != QStringLiteral("qindaqt-development-input")) {
        *error = QStringLiteral("production-gated nested development input is unavailable");
        return std::nullopt;
    }
    const auto outputs = compositor.outputs(error);
    if (!outputs || outputs->size() != 1) {
        if (error->isEmpty()) {
            *error = QStringLiteral("expected one nested logical output");
        }
        return std::nullopt;
    }
    const QJsonObject output = outputs->at(0).toObject();
    const QJsonObject geometry = output.value(QStringLiteral("geometry")).toObject();
    const double observedScale = output.value(QStringLiteral("scale")).toDouble();
    if (geometry.value(QStringLiteral("width")).toInt(-1) != expectations.logicalWidth
        || geometry.value(QStringLiteral("height")).toInt(-1) != expectations.logicalHeight
        || std::abs(observedScale - expectations.scale) > 0.0001) {
        *error = QStringLiteral("nested output geometry/scale did not match the row");
        return std::nullopt;
    }
    return QJsonObject{
        {QStringLiteral("compositorPid"), QString::number(*compositorPid)},
        {QStringLiteral("logicalWidth"), expectations.logicalWidth},
        {QStringLiteral("logicalHeight"), expectations.logicalHeight},
        {QStringLiteral("scale"), observedScale},
        {QStringLiteral("outputName"), output.value(QStringLiteral("name"))},
        {QStringLiteral("developmentInputDeviceId"),
         developmentInput.value(QStringLiteral("deviceId"))},
    };
}

} // namespace QindaQt::Test
