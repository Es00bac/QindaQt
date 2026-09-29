// SPDX-License-Identifier: GPL-3.0-or-later
#include "screenshot_app.h"

#include "capture_flow.h"
#include "capture_result.h"
#include "clipboard_publisher.h"
#include "desktop_actions.h"
#include "frame_store.h"
#include "kwin_capture_port.h"
#include "record_connection.h"
#include "record_controller.h"
#include "result_notifier.h"

#include <qindaqt/services/clipboard_wayland_adapter/production_clipboard_wayland_adapter.h>
#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/obs_provisioning.h>
#include <qindaqt/services/obs_client/obs_secret_store.h>
#include <qindaqt/services/obs_client/qt_obs_transport.h>
#include <qindaqt/services/screenshot_preferences/settings1_screenshot_preferences.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/streaming_preferences/settings1_streaming_preferences.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

#include <cstdio>
#include <memory>

namespace QindaQt::Screenshot {
namespace {

using Services::ScreenshotPreferences::Settings1ScreenshotPreferences;
using Services::StreamingPreferences::Settings1StreamingPreferences;

// Slow on purpose: a "has OBS been set up yet" watch, not a socket retry.
constexpr int ObsWatchMilliseconds = 5000;
// How long --record-toggle waits for OBS to answer before saying why not.
constexpr int RecordReadyMilliseconds = 8000;
// How long a stop waits for OBS to name the finished file.
constexpr int RecordPathMilliseconds = 10000;
constexpr int ExitCancelled = 3;

QList<ScreenGeometry> currentScreens()
{
    QList<ScreenGeometry> screens;
    for (const QScreen *screen : QGuiApplication::screens())
        screens.append({screen->name(), screen->geometry()});
    return screens;
}

std::optional<int> activeObsPort()
{
    bool found = false;
    const auto configured = Obs::readWebSocketSettings(Obs::defaultObsConfigRoot(), &found);
    return Obs::obsControlUrl(configured, found).has_value() ? std::optional<int>(configured.serverPort)
                                                             : std::nullopt;
}

} // namespace

class ScreenshotApp::Private final {
public:
    Private()
        : bus(QDBusConnection::sessionBus())
        , port(bus)
        , flow(port, frames, &currentScreens)
        , clipboard([] { return Services::ClipboardWayland::makeProductionClipboardWaylandAdapter(); })
        , notifier(bus)
        , result(frames, clipboard, notifier, DesktopActions(bus), [this] {
            return SaveTarget{preferences.folder(), preferences.fileNamePattern()};
        })
        , preferencesTransport(bus)
        , preferencesClient(preferencesTransport, Settings1ScreenshotPreferences::scopedKeys())
        , preferences(preferencesClient)
        , secrets(bus)
        , streamingTransport(bus)
        , streamingClient(streamingTransport, Settings1StreamingPreferences::scopedKeys())
        , streamingPreferences(streamingClient)
        , connection(obsClient, secrets, streamingPreferences, &activeObsPort)
        , recorder(&obsClient)
        , actions(bus)
    {
    }

    QDBusConnection bus;
    FrameStore frames;
    KWinCapturePort port;
    CaptureFlow flow;
    ClipboardPublisher clipboard;
    ResultNotifier notifier;
    CaptureResult result;
    Services::SettingsClient::QtSettingsTransport preferencesTransport;
    Services::SettingsClient::SettingsClient preferencesClient;
    Settings1ScreenshotPreferences preferences;
    Obs::QtObsTransport obsTransport;
    Obs::ObsClient obsClient{obsTransport};
    Obs::SecretServiceObsStore secrets;
    Services::SettingsClient::QtSettingsTransport streamingTransport;
    Services::SettingsClient::SettingsClient streamingClient;
    Settings1StreamingPreferences streamingPreferences;
    RecordConnection connection;
    RecordController recorder;
    DesktopActions actions;
    QTimer obsWatch;
    QString pendingRecordingPath;
};

ScreenshotApp::ScreenshotApp(Invocation invocation, QObject *parent)
    : QObject(parent)
    , m_invocation(std::move(invocation))
    , d(std::make_unique<Private>())
{
    d->notifier.start();
    QString error;
    // An absent Settings1 owner leaves the documented defaults in force.
    static_cast<void>(d->preferencesClient.start(&error));

    connect(&d->flow, &CaptureFlow::captured, this, &ScreenshotApp::onCaptured);
    connect(&d->flow, &CaptureFlow::cancelled, this, [this] { onCaptureEnded(true, {}); });
    connect(&d->flow, &CaptureFlow::failed, this,
            [this](const QString &message) { onCaptureEnded(false, message); });
    connect(&d->flow, &CaptureFlow::hideRequested, this, [this] {
        if (m_window)
            m_window->hide();
    });
    connect(&d->preferences, &Settings1ScreenshotPreferences::preferencesChanged, this,
            &ScreenshotApp::seedOptionsFromPreferences);
    connect(&d->recorder, &RecordController::recordingSaved, this, &ScreenshotApp::onRecordingSaved);

    connect(&d->notifier, &ResultNotifier::openRequested, &d->result, &CaptureResult::handleOpen);
    connect(&d->notifier, &ResultNotifier::copyImageRequested, &d->result, &CaptureResult::handleCopyFile);
    connect(&d->notifier, &ResultNotifier::showInFolderRequested, &d->result,
            &CaptureResult::handleShowInFolder);
    connect(&d->notifier, &ResultNotifier::copyPathRequested, this, &ScreenshotApp::copyRecordingPath);
    connect(&d->notifier, &ResultNotifier::settingsRequested, this, &ScreenshotApp::openStreamingSettings);
    connect(&d->result, &CaptureResult::messageChanged, this,
            [this] { setResultMessage(d->result.message()); });

    connect(&d->notifier, &ResultNotifier::liveNotificationsChanged, this, &ScreenshotApp::maybeQuit);
    connect(&d->clipboard, &ClipboardPublisher::holdsSelectionChanged, this, &ScreenshotApp::maybeQuit);
}

ScreenshotApp::~ScreenshotApp()
{
    // AGENT-GUARD: QML holds raw pointers to Private's objects through
    // context properties; the engine must die first.
    m_engine.reset();
}

bool ScreenshotApp::start()
{
    switch (m_invocation.action) {
    case Invocation::Action::Window:
        if (!loadWindow())
            return false;
        seedOptionsFromPreferences();
        showWindow();
        return true;
    case Invocation::Action::Capture:
        // The overlay (region) and the result window both live in the QML
        // scene, so it is loaded even for a windowless launch; the main
        // window just stays hidden.
        if (!loadWindow())
            return false;
        d->flow.setSettleMilliseconds(0);
        d->flow.setOptions(m_invocation.capture);
        m_optionsSeeded = true;
        m_workPending = true;
        d->flow.start();
        return true;
    case Invocation::Action::RecordToggle:
        m_workPending = true;
        runRecordToggle();
        return true;
    }
    return false;
}

bool ScreenshotApp::loadWindow()
{
    if (m_engine)
        return true;
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_engine->addImageProvider(QStringLiteral("capture"), new CaptureImageProvider(d->frames));
    QQmlContext *context = m_engine->rootContext();
    context->setContextProperty(QStringLiteral("captureFlow"), &d->flow);
    context->setContextProperty(QStringLiteral("captureResult"), &d->result);
    context->setContextProperty(QStringLiteral("recorder"), &d->recorder);
    context->setContextProperty(QStringLiteral("screenshotPreferences"), &d->preferences);
    context->setContextProperty(QStringLiteral("screenshotApp"), this);
    m_engine->loadFromModule(QStringLiteral("QindaQt.Screenshot"), QStringLiteral("Main"));
    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().value(0));
    if (!m_window) {
        std::fprintf(stderr, "qindaqt-screenshot: the window could not be loaded\n");
        m_exitCode = 1;
        return false;
    }
    connect(m_window, &QQuickWindow::closing, this, [this] {
        m_windowOpen = false;
        d->flow.cancel();
        QTimer::singleShot(0, this, &ScreenshotApp::maybeQuit);
    });
    return true;
}

void ScreenshotApp::showWindow()
{
    if (!m_window)
        return;
    m_windowOpen = true;
    // The Record tab lives in this window, so OBS is consulted only once a
    // window exists; a shortcut capture never touches the keyring.
    startObs();
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void ScreenshotApp::startObs()
{
    if (m_obsStarted)
        return;
    m_obsStarted = true;
    const auto reconcile = [this] {
        d->recorder.setGateText(RecordConnection::gateText(d->connection.reconcile()));
    };
    connect(&d->streamingPreferences, &Settings1StreamingPreferences::preferencesChanged, this, reconcile);
    QString error;
    static_cast<void>(d->streamingClient.start(&error));
    d->obsWatch.setInterval(ObsWatchMilliseconds);
    connect(&d->obsWatch, &QTimer::timeout, this, reconcile);
    d->obsWatch.start();
    reconcile();
}

void ScreenshotApp::seedOptionsFromPreferences()
{
    // AGENT-NOTE: preferences seed the window's options once, when they
    // first load; after that the window's own choices win for the session.
    if (m_optionsSeeded || !d->preferences.isLoaded())
        return;
    m_optionsSeeded = true;
    CaptureOptions options = d->flow.options();
    if (const auto mode = captureModeFromId(d->preferences.defaultMode()))
        options.mode = *mode;
    options.delaySeconds = allowedDelays().contains(d->preferences.delaySeconds())
                               ? d->preferences.delaySeconds()
                               : 0;
    d->flow.setOptions(options);
}

void ScreenshotApp::onCaptured(const QImage &image, const QString &modeId)
{
    d->result.setImage(image, modeId);
    if (m_invocation.action != Invocation::Action::Capture) {
        showWindow();
        return;
    }
    if (m_invocation.windowless()) {
        bool ok = true;
        if (m_invocation.save) {
            const QString path = d->result.saveToCommandLinePath(m_invocation.savePath);
            ok = !path.isEmpty();
            if (ok)
                std::fprintf(stdout, "%s\n", qPrintable(path));
            else
                std::fprintf(stderr, "qindaqt-screenshot: %s\n", qPrintable(d->result.message()));
        }
        if (m_invocation.copy) {
            d->result.copy();
            if (!m_invocation.save)
                d->notifier.notify(ResultKind::ScreenshotCopied, {});
        }
        finishWork(ok ? 0 : 1);
        return;
    }
    if (d->preferences.isLoaded() && !d->preferences.showResultWindow()) {
        // A shortcut capture with the result window turned off: save and
        // announce, like a windowless --save.
        finishWork(d->result.save().isEmpty() ? 1 : 0);
        return;
    }
    m_workPending = false;
    showWindow();
}

void ScreenshotApp::onCaptureEnded(bool cancelled, const QString &message)
{
    if (m_invocation.action == Invocation::Action::Capture && !m_windowOpen) {
        if (!cancelled) {
            std::fprintf(stderr, "qindaqt-screenshot: %s\n", qPrintable(message));
            d->notifier.notify(ResultKind::Failure, {}, message);
        }
        finishWork(cancelled ? ExitCancelled : 1);
        return;
    }
    setResultMessage(message);
    showWindow();
}

void ScreenshotApp::onRecordingSaved(const QString &path)
{
    d->pendingRecordingPath = path;
    const QString finish = d->preferences.recordFinish();
    if (finish == QLatin1String("quiet"))
        return;
    if (finish == QLatin1String("show-in-folder")) {
        showRecordingInFolder(path);
        return;
    }
    d->notifier.notify(ResultKind::RecordingSaved, path);
}

void ScreenshotApp::runRecordToggle()
{
    startObs();
    auto *deadline = new QTimer(this);
    deadline->setSingleShot(true);
    auto attempted = std::make_shared<bool>(false);
    const auto attempt = [this, deadline, attempted] {
        if (*attempted || !d->recorder.canToggle())
            return;
        *attempted = true;
        deadline->stop();
        if (!d->recorder.toggle()) {
            d->notifier.notify(ResultKind::RecordUnavailable, {}, d->recorder.feedback());
            finishWork(1);
        }
    };
    connect(&d->recorder, &RecordController::changed, this, attempt);
    connect(deadline, &QTimer::timeout, this, [this, attempted] {
        if (*attempted)
            return;
        *attempted = true;
        const QString why = d->recorder.statusText();
        std::fprintf(stderr, "qindaqt-screenshot: %s\n", qPrintable(why));
        d->notifier.notify(ResultKind::RecordUnavailable, {}, why);
        finishWork(1);
    });
    connect(&d->recorder, &RecordController::toggleFinished, this, [this](bool ok, bool started) {
        if (!ok) {
            d->notifier.notify(ResultKind::Failure, {}, d->recorder.feedback());
            finishWork(1);
            return;
        }
        if (started) {
            d->notifier.notify(ResultKind::RecordingStarted, {});
            finishWork(0);
            return;
        }
        // Stopped: onRecordingSaved announces the file (it may already have).
        if (!d->pendingRecordingPath.isEmpty()) {
            finishWork(0);
            return;
        }
        auto *wait = new QTimer(this);
        wait->setSingleShot(true);
        connect(&d->recorder, &RecordController::recordingSaved, wait, [this, wait] {
            wait->stop();
            finishWork(0);
        });
        connect(wait, &QTimer::timeout, this, [this] {
            d->notifier.notify(ResultKind::Failure, {}, tr("The recording stopped, but OBS did not say "
                                                           "where it saved the file."));
            finishWork(0);
        });
        wait->start(RecordPathMilliseconds);
    });
    deadline->start(RecordReadyMilliseconds);
    attempt();
}

void ScreenshotApp::openStreamingSettings()
{
    QString error;
    if (!d->actions.openSettingsPage(QStringLiteral("streaming"), &error))
        setResultMessage(error);
}

void ScreenshotApp::openCaptureSettings()
{
    openStreamingSettings();
}

void ScreenshotApp::openRecording(const QString &path)
{
    QString error;
    if (!d->actions.openFile(path, &error))
        setResultMessage(error);
}

void ScreenshotApp::showRecordingInFolder(const QString &path)
{
    QString error;
    if (!d->actions.showInFolder(path, &error))
        setResultMessage(error);
}

void ScreenshotApp::copyRecordingPath(const QString &path)
{
    if (!path.isEmpty())
        d->clipboard.copyText(path);
}

void ScreenshotApp::finishWork(int exitCode)
{
    m_exitCode = exitCode;
    m_workPending = false;
    // Let queued notification/clipboard work register its hold first.
    QTimer::singleShot(0, this, &ScreenshotApp::maybeQuit);
}

void ScreenshotApp::setResultMessage(const QString &message)
{
    if (m_resultMessage == message)
        return;
    m_resultMessage = message;
    Q_EMIT resultMessageChanged();
}

void ScreenshotApp::maybeQuit()
{
    if (m_workPending || m_windowOpen || d->flow.phase() != QLatin1String("idle")
        || d->notifier.hasLiveNotifications() || d->clipboard.holdsSelection())
        return;
    QCoreApplication::exit(m_exitCode);
}

} // namespace QindaQt::Screenshot
