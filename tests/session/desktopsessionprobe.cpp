// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
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

QJsonObject servicePending(const QString &method)
{
    return failure(QStringLiteral("service-not-ready"),
                   QStringLiteral("%1 was not sampled before service ownership completed")
                       .arg(method));
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

    // AGENT-GUARD: One probe has a one-second supervisor lifetime. Never wait
    // here for the desktop's 15-second readiness budget; emit a complete
    // pending snapshot and let the outer poller start a fresh bounded probe.
    const bool servicesReady = requiredServicesOwned(*bus);
    QJsonObject outputs = servicePending(QStringLiteral("Outputs"));
    QJsonObject inputCapabilities = servicePending(QStringLiteral("InputCapabilities"));
    QJsonObject shellVisibility = servicePending(QStringLiteral("ShellVisibilitySnapshot"));
    QJsonObject windows = servicePending(QStringLiteral("Windows"));
    QJsonObject developmentShellSurfaces =
        servicePending(QStringLiteral("DevelopmentShellSurfaces"));
    if (servicesReady) {
        QDBusInterface compositor(
            QString::fromLatin1(CompositorService),
            QString::fromLatin1(CompositorPath),
            QString::fromLatin1(CompositorInterface), connection);
        outputs = compositorCall(compositor, QStringLiteral("Outputs"));
        inputCapabilities =
            compositorCall(compositor, QStringLiteral("InputCapabilities"));
        shellVisibility =
            compositorCall(compositor, QStringLiteral("ShellVisibilitySnapshot"));
        windows = compositorCall(compositor, QStringLiteral("Windows"));
        developmentShellSurfaces =
            compositorCall(compositor, QStringLiteral("DevelopmentShellSurfaces"));
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
        {QStringLiteral("outputs"), outputs},
        {QStringLiteral("inputCapabilities"), inputCapabilities},
        {QStringLiteral("shellVisibility"), shellVisibility},
        {QStringLiteral("windows"), windows},
        // AGENT-CONTRACT: This later-integrated Notification interface must
        // expose mapped/committed `dock` records. An UnknownMethod reply is a
        // real runtime dependency failure, never permission to infer panels.
        {QStringLiteral("developmentShellSurfaces"), developmentShellSurfaces},
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
