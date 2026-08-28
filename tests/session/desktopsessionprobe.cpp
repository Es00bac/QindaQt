// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>

#include <array>
#include <unistd.h>

namespace {

constexpr auto CompositorService = "org.qindaqt.Compositor";
constexpr auto CompositorPath = "/org/qindaqt/Compositor";
constexpr auto CompositorInterface = "org.qindaqt.Compositor1";

QJsonObject failure(const QString &code, const QString &message)
{
    return {{QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("failure"),
             QJsonObject{{QStringLiteral("code"), code},
                         {QStringLiteral("message"), message}}}};
}

QJsonObject parseReply(const QDBusReply<QByteArray> &reply, const QString &method)
{
    if (!reply.isValid()) {
        return failure(QStringLiteral("dbus-call-failed"),
                       QStringLiteral("%1: %2")
                           .arg(method, reply.error().message()));
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(reply.value(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(QStringLiteral("malformed-json"),
                       QStringLiteral("%1 returned malformed JSON").arg(method));
    }
    return document.object();
}

QJsonObject compositorCall(QDBusInterface &compositor, const QString &method)
{
    return parseReply(compositor.call(method), method);
}

QJsonObject serviceRecord(QDBusConnectionInterface &bus, const QString &name)
{
    const QDBusReply<QString> owner = bus.serviceOwner(name);
    if (!owner.isValid() || owner.value().isEmpty()) {
        return {{QStringLiteral("name"), name},
                {QStringLiteral("status"), QStringLiteral("unavailable")}};
    }
    const QDBusReply<quint32> pid = bus.servicePid(owner.value());
    if (!pid.isValid() || pid.value() <= 1) {
        return {{QStringLiteral("name"), name},
                {QStringLiteral("status"), QStringLiteral("invalid-pid")},
                {QStringLiteral("owner"), owner.value()}};
    }
    return {{QStringLiteral("name"), name},
            {QStringLiteral("status"), QStringLiteral("owned")},
            {QStringLiteral("owner"), owner.value()},
            {QStringLiteral("pid"), QString::number(pid.value())}};
}

bool requiredServicesOwned(QDBusConnectionInterface &bus)
{
    constexpr std::array services{
        CompositorService,
        "org.qindaqt.Settings1",
        "org.qindaqt.Audio1",
        "org.freedesktop.Notifications",
    };
    for (const auto *service : services) {
        const QDBusReply<bool> registered =
            bus.isServiceRegistered(QString::fromLatin1(service));
        if (!registered.isValid() || !registered.value()) {
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-desktop-session-probe"));
    QDBusConnection connection = QDBusConnection::sessionBus();
    QDBusConnectionInterface *const bus = connection.interface();
    if (!connection.isConnected() || bus == nullptr) {
        QTextStream(stderr) << "private session bus is unavailable\n";
        return 2;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 15000 && !requiredServicesOwned(*bus)) {
        QThread::msleep(25);
    }

    QDBusInterface compositor(
        QString::fromLatin1(CompositorService),
        QString::fromLatin1(CompositorPath),
        QString::fromLatin1(CompositorInterface), connection);
    if (!compositor.isValid()) {
        QTextStream(stderr) << "Compositor1 is unavailable: "
                            << compositor.lastError().message() << '\n';
        return 2;
    }

    QJsonArray services;
    for (const auto *name : {CompositorService,
                             "org.qindaqt.Settings1",
                             "org.qindaqt.Audio1",
                             "org.freedesktop.Notifications"}) {
        services.append(serviceRecord(*bus, QString::fromLatin1(name)));
    }
    const QJsonObject result{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("selfPid"), QString::number(QCoreApplication::applicationPid())},
        {QStringLiteral("parentPid"), QString::number(::getppid())},
        {QStringLiteral("services"), services},
        {QStringLiteral("outputs"),
         compositorCall(compositor, QStringLiteral("Outputs"))},
        {QStringLiteral("inputCapabilities"),
         compositorCall(compositor, QStringLiteral("InputCapabilities"))},
        {QStringLiteral("shellVisibility"),
         compositorCall(compositor, QStringLiteral("ShellVisibilitySnapshot"))},
        {QStringLiteral("windows"),
         compositorCall(compositor, QStringLiteral("Windows"))},
        // AGENT-CONTRACT: This later-integrated Notification interface must
        // expose mapped/committed `dock` records. An UnknownMethod reply is a
        // real runtime dependency failure, never permission to infer panels.
        {QStringLiteral("developmentShellSurfaces"),
         compositorCall(compositor, QStringLiteral("DevelopmentShellSurfaces"))},
    };
    QTextStream stream(stdout);
    stream << "QINDAQT_DESKTOP_SESSION_PROBE="
           << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
    stream.flush();
    // Keep the probe represented in the same /proc topology snapshot that its
    // marker describes. The outer deadline remains the authoritative bound.
    QThread::msleep(250);
    return 0;
}
