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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImageReader>
#include <QPointF>
#include <QKeySequence>
#include <QTextStream>
#include <QSet>
#include <QThread>

#include <cmath>
#include <functional>
#include <optional>

namespace {

constexpr auto AudioService = "org.qindaqt.Audio1";
constexpr auto GlobalAccelService = "org.kde.kglobalaccel";
constexpr auto KWinService = "org.kde.KWin";
constexpr auto KWinCompositingPath = "/Compositor";
constexpr auto KWinCompositingInterface = "org.kde.kwin.Compositing";
constexpr auto ShellEvidenceService = "org.qindaqt.ShellDevelopment";
constexpr auto ShellEvidencePath = "/org/qindaqt/ShellDevelopment";
constexpr auto ShellEvidenceInterface = "org.qindaqt.ShellDevelopment1";
constexpr auto VolumeAction = "qindaqt_volume_up";
constexpr auto ControlsComponent = "qindaqt-desktop-controls";

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

bool hasVolumeBinding()
{
    for (const auto &shortcut : KGlobalAccel::globalShortcutsByKey(QKeySequence(Qt::Key_VolumeUp))) {
        if (shortcut.componentUniqueName() == QLatin1String(ControlsComponent)
            && shortcut.uniqueName() == QLatin1String(VolumeAction)) {
            return true;
        }
    }
    return false;
}

bool hasOpenGlCompositor(QString *error)
{
    QDBusInterface compositing(QString::fromLatin1(KWinService),
                               QString::fromLatin1(KWinCompositingPath),
                               QString::fromLatin1(KWinCompositingInterface));
    const QString type = compositing.property("compositingType").toString();
    // KWin 6.6 exports the documented public token `gl2` rather than a
    // human-readable OpenGL name on /Compositor.
    if (type != QStringLiteral("gl2") && type != QStringLiteral("gl3")) {
        *error = QStringLiteral("KWin compositingType is %1").arg(type);
        return false;
    }
    return true;
}

QString actionKeyDiagnostic(QLatin1StringView action)
{
    QDBusInterface globalAccel(QString::fromLatin1(GlobalAccelService),
                               QStringLiteral("/kglobalaccel"),
                               QStringLiteral("org.kde.KGlobalAccel"));
    const QStringList componentId{QString::fromLatin1(ControlsComponent),
                                  QString::fromLatin1(ControlsComponent)};
    const QDBusReply<QList<QStringList>> actions =
        globalAccel.call(QStringLiteral("allActionsForComponent"), componentId);
    QStringList actionId;
    if (actions.isValid()) {
        for (const QStringList &candidate : actions.value()) {
            if (candidate.size() >= 4 && candidate.at(1) == action) {
                actionId = candidate;
                break;
            }
        }
    }
    if (actionId.isEmpty()) {
        return QStringLiteral("%1 has no action identity").arg(action);
    }
    const QDBusReply<QList<int>> defaults = globalAccel.call(QStringLiteral("defaultShortcut"), actionId);
    const QDBusReply<QList<int>> active = globalAccel.call(QStringLiteral("shortcut"), actionId);
    const auto values = [](const QList<int> &keys) {
        QStringList text;
        for (const int key : keys) {
            text.append(QString::number(key));
        }
        return text.join(QLatin1Char(','));
    };
    return QStringLiteral("%1 default=[%2] active=[%3]")
        .arg(action, defaults.isValid() ? values(defaults.value()) : defaults.error().message(),
             active.isValid() ? values(active.value()) : active.error().message());
}

bool validCapturedImage(const QString &path, QString *diagnostic)
{
    QImageReader reader(path);
    const QImage image = reader.read();
    if (image.isNull() || image.width() < 32 || image.height() < 32
        || image.width() > 1920 || image.height() > 1080) {
        *diagnostic = QStringLiteral("Spectacle output is not a usable bounded image: %1")
                          .arg(reader.errorString());
        return false;
    }
    const QColor first = image.pixelColor(0, 0);
    for (const QPoint point : {QPoint(image.width() / 2, image.height() / 2),
                               QPoint(image.width() - 1, image.height() - 1)}) {
        if (image.pixelColor(point) != first) {
            return true;
        }
    }
    *diagnostic = QStringLiteral("Spectacle output is visually uniform");
    return false;
}

QString windowInventorySummary(const QJsonArray &windows)
{
    QStringList entries;
    for (const QJsonValue &entry : windows) {
        if (!entry.isObject() || entries.size() == 16) {
            continue;
        }
        const QJsonObject window = entry.toObject();
        const QJsonObject geometry = window.value(QStringLiteral("geometry")).toObject();
        entries.append(QStringLiteral("%1 title=%2 active=%3 %4x%5")
                           .arg(window.value(QStringLiteral("id")).toString(),
                                window.value(QStringLiteral("title")).toString(),
                                window.value(QStringLiteral("active")).toBool()
                                    ? QStringLiteral("true")
                                    : QStringLiteral("false"),
                                QString::number(geometry.value(QStringLiteral("width")).toDouble()),
                                QString::number(geometry.value(QStringLiteral("height")).toDouble())));
    }
    return entries.join(QStringLiteral("; "));
}

bool newFullscreenSurfaceMapped(const QJsonArray &before, const QJsonArray &after)
{
    QSet<QString> existingIds;
    for (const QJsonValue &entry : before) {
        if (entry.isObject()) {
            existingIds.insert(entry.toObject().value(QStringLiteral("id")).toString());
        }
    }
    for (const QJsonValue &entry : after) {
        if (!entry.isObject()) {
            continue;
        }
        const QJsonObject window = entry.toObject();
        const QJsonObject geometry = window.value(QStringLiteral("geometry")).toObject();
        if (!existingIds.contains(window.value(QStringLiteral("id")).toString())
            && window.value(QStringLiteral("active")).toBool()
            && geometry.value(QStringLiteral("width")).toDouble() >= 1900
            && geometry.value(QStringLiteral("height")).toDouble() >= 1080) {
            return true;
        }
    }
    return false;
}

std::optional<QString> capturedImageInDirectory(const QString &directory, QString *diagnostic)
{
    const QDir output(directory);
    const QFileInfoList candidates = output.entryInfoList(
        {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg")},
        QDir::Files | QDir::Readable, QDir::Time);
    for (const QFileInfo &candidate : candidates) {
        if (validCapturedImage(candidate.absoluteFilePath(), diagnostic)) {
            return candidate.absoluteFilePath();
        }
    }
    if (candidates.isEmpty()) {
        *diagnostic = QStringLiteral("Spectacle has not saved an image in %1").arg(directory);
    }
    return std::nullopt;
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
    QDBusReply<uint> audioPid;
    QDBusReply<uint> shellPid;
    QDBusReply<uint> globalAccelPid;
    QString error;
    if (!await([&] {
            audioPid = bus->servicePid(QString::fromLatin1(AudioService));
            shellPid = bus->servicePid(QString::fromLatin1(ShellEvidenceService));
            return audioPid.isValid() && audioPid.value() > 1 && shellPid.isValid()
                && shellPid.value() > 1;
        }, 15'000, &error)) {
        return fail(QStringLiteral("Audio1 or production shell evidence did not acquire a private owner"));
    }

    if (!await([&] {
            globalAccelPid = bus->servicePid(QString::fromLatin1(GlobalAccelService));
            return globalAccelPid.isValid() && globalAccelPid.value() > 1;
        }, 15'000, &error)) {
        return fail(QStringLiteral("private KGlobalAccel daemon did not acquire its bus name"));
    }

    if (!await([&] { return hasOpenGlCompositor(&error); }, 15'000, &error)) {
        return fail(QStringLiteral("private capture row did not acquire an OpenGL KWin backend: %1")
                        .arg(error));
    }

    if (!await([] { return hasVolumeBinding(); }, 10'000, &error)) {
        return fail(QStringLiteral("KGlobalAccel did not publish exact qindaqt-desktop-controls VolumeUp binding; observed %1")
                        .arg(actionKeyDiagnostic(QLatin1StringView(VolumeAction))));
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
    const QJsonObject beforePresentation = beforeShell->value(QStringLiteral("presentation")).toObject();
    if (beforePresentation.value(QStringLiteral("popupCount")).toInt(-1) != 0
        || beforePresentation.value(QStringLiteral("activeCount")).toInt(-1) != 0) {
        return fail(QStringLiteral("notification baseline was not clean"));
    }

    QindaQt::Test::CompositorProbeClient compositor;
    QindaQt::Test::DevelopmentInputDriver input(compositor);
    // The private fixture uses WirePlumber to set its null sink below 100%
    // before Audio1 begins. Only the real VolumeUp key changes audio state.
    if (beforeOutput.volume >= 0.999) {
        return fail(QStringLiteral("private Audio1 fixture remained saturated after WirePlumber setup"));
    }
    const auto volumeBeforeUp = beforeOutput;
    if (!input.pressKey(QLatin1StringView("volume-up"), &error)) {
        return fail(QStringLiteral("VolumeUp injection failed: %1").arg(error));
    }
    if (!await([&] {
            const auto current = defaultOutput(audio.snapshot());
            return current && current->handle == volumeBeforeUp.handle
                && current->volume > volumeBeforeUp.volume + 0.001;
        }, 8'000, &error)) {
        return fail(QStringLiteral("Audio1 default output volume did not increase after VolumeUp"));
    }
    if (!await([&] {
            QString snapshotError;
            const auto snapshot = shellSnapshot(&snapshotError);
            return snapshot && snapshot->value(QStringLiteral("presentation"))
                                   .toObject().value(QStringLiteral("popupCount")).toInt() == 1
                && snapshot->value(QStringLiteral("presentation"))
                       .toObject().value(QStringLiteral("activeCount")).toInt()
                    == 1;
        }, 4'000, &error)) {
        return fail(QStringLiteral("production VolumeUp feedback was not presented by the shell"));
    }

    const QString screenshotDirectory =
        qEnvironmentVariable("QINDAQT_DAILY_CONTROLS_SCREENSHOT_DIRECTORY");
    if (screenshotDirectory.isEmpty() || !QDir(screenshotDirectory).exists()
        || !QDir(screenshotDirectory).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        return fail(QStringLiteral("private Spectacle output directory is unavailable or not fresh"));
    }
    const auto windowsBeforePrint = compositor.windows(&error);
    if (!windowsBeforePrint) {
        return fail(QStringLiteral("could not sample windows before Print: %1").arg(error));
    }
    if (!input.pressKey(QLatin1StringView("print"), &error)) {
        return fail(QStringLiteral("Print injection failed: %1").arg(error));
    }
    // A rectangular Print launch first captures a croppable image, then maps
    // Spectacle's fullscreen CaptureWindow. Waiting for that new public KWin
    // surface prevents the development drag from racing the real selector.
    QString lastInventory;
    if (!await([&] {
            QString surfaceError;
            const auto windows = compositor.windows(&surfaceError);
            if (!windows) {
                lastInventory = surfaceError;
                return false;
            }
            lastInventory = windowInventorySummary(*windows);
            return newFullscreenSurfaceMapped(*windowsBeforePrint, *windows);
        }, 10'000, &error)) {
        return fail(QStringLiteral("Spectacle capture surface did not map before region selection; "
                                   "last public inventory: %1")
                        .arg(lastInventory));
    }
    if (!input.drag(QPointF(240, 180), QPointF(1040, 620), false, &error)) {
        return fail(QStringLiteral("Spectacle region selection injection failed: %1").arg(error));
    }
    std::optional<QString> savedCapture;
    if (!await([&] {
            savedCapture = capturedImageInDirectory(screenshotDirectory, &error);
            return savedCapture.has_value();
        }, 12'000, &error)) {
        return fail(QStringLiteral("Print did not produce a decoded Spectacle capture: %1").arg(error));
    }

    QJsonObject result{{QStringLiteral("passed"), true},
                       {QStringLiteral("audioServicePid"), static_cast<int>(audioPid.value())},
                       {QStringLiteral("globalAccelPid"), static_cast<int>(globalAccelPid.value())},
                       {QStringLiteral("shellPid"), static_cast<int>(shellPid.value())},
                       {QStringLiteral("developmentInputRequests"), input.requestCount()},
                       {QStringLiteral("volumeBefore"), beforeOutput.volume},
                       {QStringLiteral("volumeAfter"), defaultOutput(audio.snapshot())->volume},
                       {QStringLiteral("feedback"), QStringLiteral("exactly-one-production-shell-popup")},
                       {QStringLiteral("screenshot"), QStringLiteral("decoded-region-capture")},
                       {QStringLiteral("screenshotPath"), *savedCapture}};
    QTextStream(stdout) << "QINDAQT_DAILY_CONTROLS="
                        << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
    return 0;
}
