// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopnotificationbinding.h"
#include "desktopnotificationshellreadiness.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <array>
#include <unistd.h>

namespace {

constexpr auto CompositorService = "org.qindaqt.Compositor";
constexpr auto CompositorPath = "/org/qindaqt/Compositor";
constexpr auto CompositorInterface = "org.qindaqt.Compositor1";
constexpr int ShortcutReadinessDeadlineMilliseconds = 1'000;
constexpr int ShortcutReadinessPollMilliseconds = 20;

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

QJsonObject compositorCall(QDBusInterface &compositor, const QString &method,
                           const QByteArray &argument)
{
    return parseReply(compositor.call(method, argument), method);
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

QJsonObject shellPending(const QString &message)
{
    return {
        {QStringLiteral("status"), QStringLiteral("pending")},
        {QStringLiteral("failure"),
             QJsonObject{{QStringLiteral("code"),
                      QStringLiteral("public-topology-pending")},
                     {QStringLiteral("message"), message}}},
    };
}

QJsonObject keyEvent(QLatin1StringView key, bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("key")},
            {QStringLiteral("key"), key},
            {QStringLiteral("pressed"), pressed}};
}

QJsonObject pointerAbsoluteEvent(double x, double y)
{
    return {{QStringLiteral("type"), QStringLiteral("pointer-absolute")},
            {QStringLiteral("x"), x},
            {QStringLiteral("y"), y}};
}

std::optional<QindaQt::Test::DesktopNotificationBinding>
awaitNotificationCenterBinding(QString *error)
{
    QElapsedTimer deadline;
    deadline.start();
    do {
        auto binding = QindaQt::Test::queryDesktopNotificationBinding(error);
        if (binding) {
            return binding;
        }
        if (deadline.elapsed() >= ShortcutReadinessDeadlineMilliseconds) {
            break;
        }
        // AGENT-GUARD: This event-loop wait observes binding publication; it is
        // not a fixed startup delay and must remain before the one input batch.
        // Retrying Meta+N would make a lost event indistinguishable from proof.
        QEventLoop waitForPublication;
        QTimer::singleShot(ShortcutReadinessPollMilliseconds,
                           &waitForPublication, &QEventLoop::quit);
        waitForPublication.exec();
    } while (true);
    return std::nullopt;
}

int runNotificationCenterInteraction(QDBusConnectionInterface &bus,
                                     const QDBusConnection &connection,
                                     const QString &targetOutput = {})
{
    if (!requiredServicesOwned(bus)) {
        QTextStream(stderr) << "required private services are unavailable\n";
        return 3;
    }
    QString bindingError;
    const auto initialBinding = awaitNotificationCenterBinding(&bindingError);
    if (!initialBinding) {
        QTextStream(stderr)
            << "notification-center Meta+N binding was not ready before private input: "
            << bindingError << '\n';
        return 9;
    }
    QDBusInterface compositor(QString::fromLatin1(CompositorService),
                              QString::fromLatin1(CompositorPath),
                              QString::fromLatin1(CompositorInterface), connection);
    const QJsonObject before =
        compositorCall(compositor, QStringLiteral("DevelopmentShellSurfaces"));
    const QJsonValue beforeSurfaces = before.value(QStringLiteral("surfaces"));
    if (before.value(QStringLiteral("status")) != QStringLiteral("ok")
        || !beforeSurfaces.isArray()) {
        QTextStream(stderr) << "pre-injection shell-surface evidence is unavailable\n";
        return 4;
    }
    const QJsonObject interactionOutputs =
        compositorCall(compositor, QStringLiteral("Outputs"));
    QString shellError;
    const auto shellExpectation =
        QindaQt::Test::desktopNotificationShellExpectation(
            before, interactionOutputs, targetOutput,
            QindaQt::Test::DesktopNotificationShellPhase::ClosedHidden, 0,
            &shellError);
    if (!shellExpectation.ready()) {
        QTextStream(stderr)
            << "notification shell readiness evidence is unavailable: "
            << shellError << '\n';
        return 11;
    }
    int preInjectionActiveSurfaceCount = 0;
    for (const QJsonValue &value : beforeSurfaces.toArray()) {
        const QJsonObject surface = value.toObject();
        if (surface.value(QStringLiteral("scope")) == QStringLiteral("notification-center")
            && surface.value(QStringLiteral("mapped")).toBool()
            && surface.value(QStringLiteral("committed")).toBool()
            && surface.value(QStringLiteral("active")).toBool()) {
            ++preInjectionActiveSurfaceCount;
        }
    }
    if (preInjectionActiveSurfaceCount != 0) {
        QTextStream(stderr) << "notification center was active before private input\n";
        return 5;
    }
    QJsonArray events;
    if (!targetOutput.isEmpty()) {
        const QJsonObject &outputs = interactionOutputs;
        const QJsonValue outputValues = outputs.value(QStringLiteral("outputs"));
        if (outputs.value(QStringLiteral("status")) != QStringLiteral("ok")
            || !outputValues.isArray()) {
            QTextStream(stderr) << "secondary output evidence is unavailable\n";
            return 6;
        }
        QJsonObject targetGeometry;
        int targetMatches = 0;
        for (const QJsonValue &value : outputValues.toArray()) {
            const QJsonObject output = value.toObject();
            if (output.value(QStringLiteral("name")) == targetOutput) {
                targetGeometry = output.value(QStringLiteral("geometry")).toObject();
                ++targetMatches;
            }
        }
        const int x = targetGeometry.value(QStringLiteral("x")).toInt();
        const int y = targetGeometry.value(QStringLiteral("y")).toInt();
        const int width = targetGeometry.value(QStringLiteral("width")).toInt();
        const int height = targetGeometry.value(QStringLiteral("height")).toInt();
        if (targetMatches != 1 || width <= 0 || height <= 0) {
            QTextStream(stderr) << "secondary output target is unavailable\n";
            return 6;
        }
        // AGENT-CONTRACT: Moving the private pointer into WL-1 is the fifth
        // dual-row event. KWin selects global-shortcut presentation from this
        // seat/output context; the later surface check must still prove that
        // the shell committed the notification center to that exact output.
        events.append(pointerAbsoluteEvent(
            static_cast<double>(x) + static_cast<double>(width) / 2.0,
            static_cast<double>(y) + static_cast<double>(height) / 2.0));
    }
    events.append(keyEvent(QLatin1StringView("left-meta"), true));
    events.append(keyEvent(QLatin1StringView("n"), true));
    events.append(keyEvent(QLatin1StringView("n"), false));
    events.append(keyEvent(QLatin1StringView("left-meta"), false));

    // AGENT-GUARD: Registry keys can remain visible while their owning
    // component is inactive. Re-authenticate the exact live component at the
    // last boundary before the sole target batch; never replace this with a
    // startup delay or a second Meta+N attempt.
    const auto binding = QindaQt::Test::queryDesktopNotificationBinding(
        &bindingError);
    if (!binding
        || binding->componentObjectPath != initialBinding->componentObjectPath) {
        QTextStream(stderr)
            << "notification-center component was not stably active before private input: "
            << bindingError << '\n';
        return 9;
    }
    QindaQt::Test::DesktopNotificationActivationObserver activationObserver;
    if (!activationObserver.start(connection, *binding, &bindingError)) {
        QTextStream(stderr)
            << "notification-center activation evidence was unavailable before private input: "
            << bindingError << '\n';
        return 9;
    }

    // This is the last state observation before the sole input batch. It is a
    // single authenticated sample, never an inner readiness poll.
    const auto shellBefore = QindaQt::Test::sampleDesktopNotificationShell(
        connection, bus, *shellExpectation.expectation);
    if (!shellBefore.ready()) {
        QTextStream(stderr)
            << "notification shell was not presentation-ready before private input: "
            << shellBefore.message << '\n';
        return 11;
    }
    const quint64 centerOpenedCountBefore =
        shellBefore.evidence.value(QStringLiteral("centerOpenedCount"))
            .toString()
            .toULongLong();
    const QindaQt::Test::DesktopNotificationShellExpectation
        openedShellExpectation{
            shellExpectation.expectation->dockProcessId,
            shellExpectation.expectation->outputName,
            shellBefore.evidence.value(QStringLiteral("owner")).toString(),
            QindaQt::Test::DesktopNotificationShellPhase::OpenVisible,
            centerOpenedCountBefore,
        };

    const QJsonObject request{{QStringLiteral("schemaVersion"), 1},
                              {QStringLiteral("events"), events}};
    const QJsonObject injected = compositorCall(
        compositor, QStringLiteral("InjectTestInput"),
        QJsonDocument(request).toJson(QJsonDocument::Compact));
    if (injected.value(QStringLiteral("status")) != QStringLiteral("injected")
        || injected.value(QStringLiteral("eventCount")).toInt(-1) != events.size()
        || injected.value(QStringLiteral("deviceId"))
               != QStringLiteral("qindaqt-development-input")) {
        QTextStream(stderr) << "private development input was not accepted\n";
        return 6;
    }
    // The action travels through KGlobalAccel and the shell, so observe the
    // exact component press/release and compositor-owned surface instead of
    // assuming metadata or synchronous UI work proves target delivery.
    QindaQt::Test::DesktopNotificationShellCheck shellAfter;
    for (int attempt = 0; attempt != 60; ++attempt) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        shellAfter = QindaQt::Test::sampleDesktopNotificationShell(
            connection, bus, openedShellExpectation);
        if (shellAfter.disposition
            == QindaQt::Test::DesktopNotificationShellDisposition::Invalid) {
            QTextStream(stderr)
                << "notification shell presentation evidence became invalid: "
                << shellAfter.message << '\n';
            return 11;
        }
        const QJsonObject inventory =
            compositorCall(compositor, QStringLiteral("DevelopmentShellSurfaces"));
        if (inventory.value(QStringLiteral("status")) != QStringLiteral("ok")
            || !inventory.value(QStringLiteral("surfaces")).isArray()) {
            QTextStream(stderr) << "post-injection shell-surface evidence is unavailable\n";
            return 7;
        }
        QJsonObject match;
        int matches = 0;
        for (const QJsonValue &value : inventory.value(QStringLiteral("surfaces")).toArray()) {
            const QJsonObject surface = value.toObject();
            const QJsonObject geometry = surface.value(QStringLiteral("geometry")).toObject();
            if (surface.value(QStringLiteral("scope")) == QStringLiteral("notification-center")
                && surface.value(QStringLiteral("processId"))
                    == QString::number(openedShellExpectation.dockProcessId)
                && surface.value(QStringLiteral("mapped")).toBool()
                && surface.value(QStringLiteral("committed")).toBool()
                && surface.value(QStringLiteral("active")).toBool()
                && surface.value(QStringLiteral("outputName"))
                    == openedShellExpectation.outputName
                && surface.value(QStringLiteral("desiredOutputName"))
                    == openedShellExpectation.outputName
                && geometry.value(QStringLiteral("width")).toInt() == 440
                && geometry.value(QStringLiteral("height")).toInt() == 640) {
                match = surface;
                ++matches;
            }
        }
        if (matches == 1 && activationObserver.complete() && shellAfter.ready()) {
            const QJsonObject activation{
                {QStringLiteral("action"),
                 QindaQt::Test::desktopNotificationActionId()},
                {QStringLiteral("component"),
                 QindaQt::Test::desktopNotificationComponentId()},
                {QStringLiteral("pressed"), true},
                {QStringLiteral("released"), true},
            };
            const QJsonObject result{
                {QStringLiteral("action"), QStringLiteral("open-notification-center")},
                {QStringLiteral("deviceId"), injected.value(QStringLiteral("deviceId"))},
                {QStringLiteral("eventCount"), events.size()},
                {QStringLiteral("activation"), activation},
                {QStringLiteral("preInjectionActiveSurfaceCount"),
                 preInjectionActiveSurfaceCount},
                {QStringLiteral("shellPresentation"),
                 QJsonObject{
                     {QStringLiteral("before"), shellBefore.evidence},
                     {QStringLiteral("after"), shellAfter.evidence},
                 }},
                {QStringLiteral("surface"), match},
            };
            QTextStream(stdout)
                << "QINDAQT_DESKTOP_NOTIFICATION_ACTIVATION="
                << QJsonDocument(activation).toJson(QJsonDocument::Compact)
                << '\n';
            QTextStream(stdout) << "QINDAQT_DESKTOP_SESSION_INTERACTION="
                                << QJsonDocument(result).toJson(QJsonDocument::Compact)
                                << '\n';
            return 0;
        }
        QEventLoop observeDelivery;
        QTimer::singleShot(50, &observeDelivery, &QEventLoop::quit);
        observeDelivery.exec();
    }
    if (!activationObserver.complete()) {
        QTextStream(stderr)
            << "notification shortcut activation was not delivered by KGlobalAccel: "
            << activationObserver.diagnostic() << '\n';
        return 10;
    }
    if (!shellAfter.ready()) {
        QTextStream(stderr)
            << "notification action did not open the shell presentation: "
            << shellAfter.message << '\n';
        return 11;
    }
    QTextStream(stderr) << "notification center did not map on the private seat\n";
    return 8;
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
    if (application.arguments().size() == 2) {
        const QString interaction = application.arguments().at(1);
        if (interaction == QStringLiteral("--open-notification-center")) {
            return runNotificationCenterInteraction(*bus, connection);
        }
        if (interaction == QStringLiteral("--open-notification-center-secondary")) {
            return runNotificationCenterInteraction(*bus, connection,
                                                    QStringLiteral("WL-1"));
        }
    }
    if (application.arguments().size() != 1) {
        QTextStream(stderr) << "unsupported probe arguments\n";
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
    QJsonObject notificationShell =
        shellPending(QStringLiteral("public shell topology is not ready"));
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
        QString shellError;
        const auto expectation =
            QindaQt::Test::desktopNotificationShellExpectation(
                developmentShellSurfaces, outputs, {},
                QindaQt::Test::DesktopNotificationShellPhase::ClosedHidden, 0,
                &shellError);
        notificationShell = expectation.ready()
            ? QindaQt::Test::sampleDesktopNotificationShell(
                  connection, *bus, *expectation.expectation).document()
            : expectation.document();
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
        // The outer 15-second poll owns every retry. This fixed-lifetime probe
        // contributes exactly one authenticated shell presentation sample.
        {QStringLiteral("notificationShell"), notificationShell},
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
