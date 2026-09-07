// SPDX-License-Identifier: GPL-3.0-or-later

#include "compositorprobeclient.h"
#include "hybridtestinputdriver.h"

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_client/qt_audio_transport.h>

#include <KGlobalAccel>
#include <KGlobalShortcutInfo>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QTextStream>
#include <QThread>

#include <cmath>
#include <functional>
#include <optional>

namespace {

constexpr auto AudioService = "org.qindaqt.Audio1";
constexpr auto ShellEvidenceService = "org.qindaqt.ShellDevelopment";
constexpr auto ShellEvidencePath = "/org/qindaqt/ShellDevelopment";
constexpr auto ShellEvidenceInterface = "org.qindaqt.ShellDevelopment1";
constexpr auto VolumeAction = "qindaqt_volume_up";
constexpr auto ScreenshotAction = "qindaqt_take_screenshot";

bool await(const std::function<bool()> &condition, const int timeout, QString *error)
{
    QElapsedTimer deadline;
    deadline.start();
    while (deadline.elapsed() < timeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (condition()) {
            error->clear();
            return true;
        }
        QThread::msleep(20);
    }
    if (condition()) {
        error->clear();
        return true;
    }
    return false;
}

std::optional<QJsonObject> shellSnapshot(QString *error)
{
    QDBusInterface shell(QString::fromLatin1(ShellEvidenceService),
                         QString::fromLatin1(ShellEvidencePath),
                         QString::fromLatin1(ShellEvidenceInterface));
    const QDBusReply<QByteArray> reply = shell.call(QStringLiteral("Snapshot"));
    if (!reply.isValid()) {
        *error = QStringLiteral("shell evidence Snapshot failed: %1").arg(reply.error().message());
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value(QStringLiteral("schemaVersion")).toInt() != 1) {
        *error = QStringLiteral("shell evidence Snapshot is malformed: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    return document.object();
}

std::optional<QindaQt::Audio::Device> defaultOutput(const QindaQt::Audio::Snapshot &snapshot)
{
    for (const auto &output : snapshot.outputs) {
        if (output.handle == snapshot.defaultOutput && output.volumeKnown && output.canSetVolume) {
            return output;
        }
    }
    return std::nullopt;
}

bool hasBinding(const QKeySequence &key, QLatin1StringView action)
{
    for (const auto &shortcut : KGlobalAccel::globalShortcutsByKey(key)) {
        if (shortcut.uniqueName() == action) {
            return true;
        }
    }
    return false;
}

bool hasSpectacleSurface(QindaQt::Test::CompositorProbeClient &compositor, QString *diagnostic)
{
    const auto windows = compositor.windows(diagnostic);
    if (!windows) {
        return false;
    }
    for (const QJsonValue &value : *windows) {
        const QJsonObject window = value.toObject();
        const QString title = window.value(QStringLiteral("title")).toString();
        // A visible client surface is the real selection/output UI. This does
        // not accept a detached PID or command-line claim as capture evidence.
        if (title.contains(QStringLiteral("Spectacle"), Qt::CaseInsensitive)
            && !window.value(QStringLiteral("hidden")).toBool()
            && !window.value(QStringLiteral("minimized")).toBool()) {
            return true;
        }
    }
    *diagnostic = QStringLiteral("no visible Spectacle capture surface");
    return false;
}

int fail(const QString &message)
{
    QTextStream(stderr) << "daily-controls probe failed: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.process(application);

    if (qEnvironmentVariable("QINDAQT_DAILY_CONTROLS_PRIVATE_BUS") != QLatin1String("1")
        || qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS")
        || !qEnvironmentVariableIsEmpty("DISPLAY")
        || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        return fail(QStringLiteral("refused a non-private or inherited desktop environment"));
    }
    auto *const bus = QDBusConnection::sessionBus().interface();
    if (bus == nullptr || !QDBusConnection::sessionBus().isConnected()) {
        return fail(QStringLiteral("private session bus is unavailable"));
    }
    const QDBusReply<uint> audioPid = bus->servicePid(QString::fromLatin1(AudioService));
    const QDBusReply<uint> shellPid = bus->servicePid(QString::fromLatin1(ShellEvidenceService));
    if (!audioPid.isValid() || audioPid.value() <= 1 || !shellPid.isValid() || shellPid.value() <= 1) {
        return fail(QStringLiteral("Audio1 or production shell evidence has no private owner"));
    }

    QString error;
    if (!await([] {
            return hasBinding(QKeySequence(Qt::Key_VolumeUp), QLatin1StringView(VolumeAction))
                && hasBinding(QKeySequence(Qt::Key_Print), QLatin1StringView(ScreenshotAction));
        }, 10'000, &error)) {
        return fail(QStringLiteral("real KGlobalAccel did not publish VolumeUp and Print bindings"));
    }

    QindaQt::Audio::QtAudioTransport transport(QDBusConnection::sessionBus());
    QindaQt::Audio::AudioClient audio(&transport);
    audio.start();
    if (!await([&] { return audio.hasSnapshot() && defaultOutput(audio.snapshot()).has_value(); },
               15'000, &error)) {
        return fail(QStringLiteral("Audio1 did not publish an adjustable default private output"));
    }
    const auto beforeOutput = *defaultOutput(audio.snapshot());
    const auto beforeShell = shellSnapshot(&error);
    if (!beforeShell) {
        return fail(error);
    }
    const int beforePopupCount = beforeShell->value(QStringLiteral("presentation"))
                                     .toObject().value(QStringLiteral("popupCount")).toInt(-1);

    QindaQt::Test::CompositorProbeClient compositor;
    QindaQt::Test::DevelopmentInputDriver input(compositor);
    if (!input.pressKey(QLatin1StringView("volume-up"), &error)) {
        return fail(QStringLiteral("VolumeUp injection failed: %1").arg(error));
    }
    if (!await([&] {
            const auto current = defaultOutput(audio.snapshot());
            return current && current->handle == beforeOutput.handle
                && current->volume > beforeOutput.volume + 0.001;
        }, 8'000, &error)) {
        return fail(QStringLiteral("Audio1 default output volume did not increase after VolumeUp"));
    }
    if (!await([&] {
            QString snapshotError;
            const auto snapshot = shellSnapshot(&snapshotError);
            return snapshot && snapshot->value(QStringLiteral("presentation"))
                                   .toObject().value(QStringLiteral("popupCount")).toInt()
                    > beforePopupCount;
        }, 4'000, &error)) {
        return fail(QStringLiteral("production volume feedback was not presented by the shell"));
    }

    if (!input.pressKey(QLatin1StringView("print"), &error)) {
        return fail(QStringLiteral("Print injection failed: %1").arg(error));
    }
    if (!await([&] { return hasSpectacleSurface(compositor, &error); }, 12'000, &error)) {
        return fail(QStringLiteral("Print did not open a real Spectacle capture surface: %1").arg(error));
    }

    QJsonObject result{{QStringLiteral("passed"), true},
                       {QStringLiteral("audioServicePid"), static_cast<int>(audioPid.value())},
                       {QStringLiteral("shellPid"), static_cast<int>(shellPid.value())},
                       {QStringLiteral("developmentInputRequests"), input.requestCount()},
                       {QStringLiteral("volumeBefore"), beforeOutput.volume},
                       {QStringLiteral("volumeAfter"), defaultOutput(audio.snapshot())->volume},
                       {QStringLiteral("feedback"), QStringLiteral("production-shell-popup")},
                       {QStringLiteral("screenshot"), QStringLiteral("visible-spectacle-capture-surface")}};
    QTextStream(stdout) << "QINDAQT_DAILY_CONTROLS="
                        << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
    return 0;
}
