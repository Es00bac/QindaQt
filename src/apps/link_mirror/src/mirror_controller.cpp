// SPDX-License-Identifier: GPL-3.0-or-later
#include "mirror_controller.h"

#include "ffmpeg_decoder.h"

#include <QIcon>
#include <QPixmap>

#include <cmath>

namespace qindaqt::link_mirror {
namespace {

// AGENT-NOTE: a window edge drag emits dozens of resizes per second; each server resize
// restarts the encoder and makes the app re-layout. 250 ms keeps drags smooth and cheap.
constexpr int kResizeDebounceMs = 250;
constexpr int kListIconSize = 64;
// The server ends a greeted session after 30 s without a frame (ListenPolicy.IDLE_TIMEOUT_MS).
constexpr int kKeepaliveMs = 5000;
// How long "App closed on phone" stays visible before the window closes itself.
constexpr int kAppClosedLingerMs = 2000;
// Exit status of a headless run whose app closed on the phone before the run finished.
constexpr int kExitAppClosed = 3;
constexpr int kWindowIconSize = 128;

QSize devicePixels(QSize logical, qreal ratio)
{
    return QSize(static_cast<int>(std::lround(logical.width() * ratio)),
                 static_cast<int>(std::lround(logical.height() * ratio)));
}

} // namespace

MirrorController::MirrorController(const MirrorOptions &options, MirrorWindow &window, ViewportMapper &mapper,
                                   QObject *parent)
    : QObject(parent), m_options(options), m_window(window), m_mapper(mapper),
      m_adb(options.adbPath.isEmpty() ? AdbSession::defaultAdbPath() : options.adbPath, options.serial),
      m_worker(std::make_unique<DecodeWorker>(options.decoder, &makeFfmpegDecoder))
{
    m_resizeDebounce.setSingleShot(true);
    m_resizeDebounce.setInterval(kResizeDebounceMs);
    connect(&m_resizeDebounce, &QTimer::timeout, this, &MirrorController::applyResize);
    m_keepalive.setInterval(kKeepaliveMs);
    connect(&m_keepalive, &QTimer::timeout, this, [this] { m_connection.send(pingFrame()); });

    connect(&m_adb, &AdbSession::serverLine, this, [this](const QString &line) {
        if (m_options.verbose) {
            report(line);
        }
    });
    connect(&m_adb, &AdbSession::serverExited, this, [this](int exitCode) {
        // Only a server that dies before ready is a start failure; after ready, the
        // connection's own disconnect (or app-exited) decides the outcome.
        if (!m_sessionReady) {
            finish(1, QStringLiteral("the phone mirror server exited with code %1").arg(exitCode));
        }
    });
    connect(&m_connection, &MirrorConnection::ready, this, &MirrorController::onReady);
    connect(&m_connection, &MirrorConnection::displayChanged, this, &MirrorController::onDisplay);
    connect(&m_connection, &MirrorConnection::serverError, this, &MirrorController::onServerError);
    connect(&m_connection, &MirrorConnection::appExited, this, &MirrorController::onAppExited);
    connect(&m_connection, &MirrorConnection::appReceived, this, &MirrorController::onApp);
    connect(&m_connection, &MirrorConnection::inventoryFinished, this, &MirrorController::inventoryDone);
    connect(&m_connection, &MirrorConnection::videoFormat, this,
            [this](const VideoFormat &format) { m_worker->postFormat(format); });
    connect(&m_connection, &MirrorConnection::videoPacket, this,
            [this](const VideoPacket &packet) { m_worker->postPacket(packet); });
    connect(&m_connection, &MirrorConnection::failed, this, [this](const QString &reason) { finish(1, reason); });
    connect(&m_connection, &MirrorConnection::disconnected, this, [this] {
        // AGENT-GUARD: the server closes right after app-exited by design; that close is not
        // an ADB loss and must not replace "App closed on phone" or the clean exit status.
        if (m_appExited) {
            return;
        }
        finish(1, QStringLiteral("the phone mirror ended; streaming needs ADB to stay connected"));
    });

    connect(m_worker.get(), &DecodeWorker::frameAvailable, this, [this] {
        const QImage frame = m_worker->takeFrame();
        if (!frame.isNull() && !m_finished) {
            m_window.setFrame(frame);
            Q_EMIT frameShown();
        }
    });
    connect(m_worker.get(), &DecodeWorker::decoderOpened, this, [this](DecoderPath path, QSize size) {
        m_decoderPath = path;
        if (size.isValid()) {
            m_streamSize = size;
        }
        if (m_options.verbose) {
            report(QStringLiteral("decoder %1 %2x%3").arg(decoderPathName(path)).arg(size.width()).arg(size.height()));
        }
    });
    connect(m_worker.get(), &DecodeWorker::keyframeNeeded, this, [this] { m_connection.send(syncFrame()); });
    connect(m_worker.get(), &DecodeWorker::decoderFailed, this,
            [this](const QString &reason) { finish(1, QStringLiteral("no video decoder: ") + reason); });
    connect(m_worker.get(), &DecodeWorker::decodeWarning, this, [this](const QString &reason) {
        if (m_options.verbose) {
            report(reason);
        }
    });

    connect(&m_window, &MirrorWindow::inputFrames, this, [this](const QList<QByteArray> &frames) {
        for (const QByteArray &frame : frames) {
            m_connection.send(frame);
        }
    });
    connect(&m_window, &MirrorWindow::viewResized, this, &MirrorController::handleViewResized);
    connect(&m_window, &MirrorWindow::closeRequested, this, [this] { finish(0); });
    // finish(0) after a shown failure keeps that failure's exit code (see finish()).
}

MirrorController::~MirrorController()
{
    finish(0);
}

void MirrorController::start(QSize initialViewSize, qreal devicePixelRatio)
{
    m_pendingView = initialViewSize;
    m_pendingRatio = devicePixelRatio;
    m_window.setStatusText(QStringLiteral("Connecting to the phone…"));
    QString error;
    // AGENT-GUARD: the secret travels only on the server's stdin and inside hello; argv, the
    // socket name and the forward carry the non-secret suffix. Neither is ever logged.
    const QString secret = AdbSession::randomHex(16);
    const QString socketSuffix = AdbSession::randomHex(8);
    const auto classPath = m_adb.resolveClassPath(m_options.serverJar, &error);
    if (!classPath || !m_adb.startServer(*classPath, socketSuffix, secret, m_options.verbose, &error)) {
        finish(1, error);
        return;
    }
    const auto port = m_adb.forward(socketSuffix, &error);
    if (!port) {
        finish(1, error);
        return;
    }
    m_connection.connectTo(*port, secret, m_options.connectTimeoutMs);
}

void MirrorController::onReady(const ReadyInfo &info)
{
    if (m_options.verbose) {
        report(QStringLiteral("server ready: protocol %1, SDK %2").arg(info.version).arg(info.sdk));
    }
    m_sessionReady = true;
    m_keepalive.start();
    if (m_options.list) {
        m_connection.send(inventoryListFrame({}, kListIconSize));
        return;
    }
    StartRequest request;
    request.packageName = m_options.packageName;
    request.size = devicePixels(m_pendingView, m_pendingRatio);
    request.dpi = densityFor(m_pendingRatio);
    request.codec = m_options.codec;
    request.bitrateBps = m_options.bitrateBps;
    request.maxFps = m_options.maxFps;
    m_requestedDisplay = request.size;
    m_requestedDpi = request.dpi;
    m_connection.send(startFrame(request));
    m_connection.send(inventoryListFrame({m_options.packageName}, kWindowIconSize));
    m_started = true;
}

void MirrorController::onDisplay(const DisplayInfo &display)
{
    m_mapper.setDisplaySize(display.size);
    m_window.setStatusText(QString());
    Q_EMIT displayReady(display);
}

void MirrorController::onApp(const AppEntry &app)
{
    if (m_options.list) {
        Q_EMIT appListed(app);
        return;
    }
    if (app.packageName != m_options.packageName) {
        return;
    }
    m_window.setTitle(app.label);
    if (!app.icon.isNull()) {
        m_window.setIcon(QIcon(QPixmap::fromImage(app.icon)));
    }
}

void MirrorController::onServerError(const ServerError &error)
{
    if (error.fatal) {
        finish(1, QStringLiteral("phone: %1: %2").arg(error.code, error.message));
    } else if (m_options.verbose) {
        report(QStringLiteral("phone warning %1: %2").arg(error.code, error.message));
    }
}

void MirrorController::onAppExited(const QString &reason)
{
    if (m_finished) {
        return;
    }
    m_appExited = true;
    report(QStringLiteral("the app closed on the phone (%1)").arg(reason));
    if (m_options.headless()) {
        finish(kExitAppClosed, QStringLiteral("the app closed on the phone before the run finished"));
        return;
    }
    m_window.setFrame(QImage());
    m_window.setStatusText(QStringLiteral("App closed on phone"));
    QTimer::singleShot(kAppClosedLingerMs, this, [this] { finish(0); });
}

void MirrorController::handleViewResized(QSize logicalSize, qreal devicePixelRatio)
{
    m_pendingView = logicalSize;
    m_pendingRatio = devicePixelRatio;
    if (m_started) {
        m_resizeDebounce.start();
    }
}

void MirrorController::applyResize()
{
    const QSize wanted = devicePixels(m_pendingView, m_pendingRatio);
    const int dpi = densityFor(m_pendingRatio);
    if (wanted.isEmpty() || (wanted == m_requestedDisplay && dpi == m_requestedDpi)) {
        return;
    }
    m_requestedDisplay = wanted;
    m_requestedDpi = dpi;
    m_connection.send(resizeFrame(wanted, dpi));
}

int MirrorController::densityFor(qreal devicePixelRatio) const
{
    // 160 dpi is Android's 1 dp == 1 px baseline, which matches desktop UI scale at ratio 1.
    return m_options.dpi > 0 ? m_options.dpi : static_cast<int>(std::lround(160.0 * devicePixelRatio));
}

void MirrorController::finish(int exitCode, const QString &reason)
{
    if (m_finished) {
        // A second call (window closed, Ctrl+C) while a failure is on screen ends the process.
        if (m_waitingForClose) {
            m_waitingForClose = false;
            Q_EMIT finished(m_exitCode);
        }
        return;
    }
    m_finished = true;
    m_exitCode = exitCode;
    m_resizeDebounce.stop();
    m_keepalive.stop();
    if (!reason.isEmpty()) {
        report(reason);
        if (exitCode != 0) {
            for (const QString &line : m_adb.recentServerOutput()) {
                report(QStringLiteral("  phone: ") + line);
            }
        }
    }
    m_connection.close();
    m_adb.stop();
    // AGENT-NOTE: ADR-0433 requires the UI to say why streaming stopped (for example ADB
    // unplugged) instead of vanishing; a visible window keeps the reason until it is closed.
    if (exitCode != 0 && !reason.isEmpty() && !m_options.headless() && m_window.isVisible()) {
        m_window.setFrame(QImage());
        m_window.setStatusText(reason);
        m_waitingForClose = true;
        return;
    }
    Q_EMIT finished(exitCode);
}

void MirrorController::report(const QString &message) const
{
    qInfo("qindaqt-link-mirror: %s", qPrintable(message));
}

} // namespace qindaqt::link_mirror
