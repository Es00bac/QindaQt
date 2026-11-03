// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "adb_session.h"
#include "decode_worker.h"
#include "mirror_connection.h"
#include "mirror_options.h"
#include "mirror_window.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <memory>

// Orchestrates one mirror: ADB session -> connection -> start -> decode -> present, plus
// resize debouncing and cleanup.
//
// Ownership: owns the AdbSession, MirrorConnection and DecodeWorker; borrows the window and
// mapper, which outlive it (main.cpp). finish() is idempotent and always removes the forward
// and ends the server process before emitting finished().
//
// Threading: GUI thread.
namespace qindaqt::link_mirror {

class MirrorController : public QObject {
    Q_OBJECT

public:
    MirrorController(const MirrorOptions &options, MirrorWindow &window, ViewportMapper &mapper,
                     QObject *parent = nullptr);
    ~MirrorController() override;

    // initialViewSize/dpr describe the window the stream should fill.
    void start(QSize initialViewSize, qreal devicePixelRatio);
    void finish(int exitCode, const QString &reason = {});
    // Window resize entry point (also used by --input resize steps); debounced.
    void handleViewResized(QSize logicalSize, qreal devicePixelRatio);

    [[nodiscard]] DecoderPath decoderPath() const { return m_decoderPath; }
    [[nodiscard]] QSize streamSize() const { return m_streamSize; }
    [[nodiscard]] quint64 decodedFrames() const { return m_worker->decodedFrames(); }
    [[nodiscard]] quint64 droppedPackets() const { return m_worker->droppedPackets(); }
    [[nodiscard]] bool paused() const { return m_window.paused(); }
    [[nodiscard]] int keyframeRequests() const { return m_keyframeRequests; }

Q_SIGNALS:
    void displayReady(const qindaqt::link_mirror::DisplayInfo &display);
    void frameShown();
    void appListed(const qindaqt::link_mirror::AppEntry &app);
    void inventoryDone(int count);
    void finished(int exitCode);
    void pausedChanged(bool paused);

private:
    void onReady(const ReadyInfo &info);
    void onDisplay(const DisplayInfo &display);
    void onApp(const AppEntry &app);
    void onServerError(const ServerError &error);
    void onAppExited(const QString &reason);
    void setPaused(bool paused);
    void requestKeyframe();
    void applyResize();
    [[nodiscard]] int densityFor(qreal devicePixelRatio) const;
    void report(const QString &message) const;

    MirrorOptions m_options;
    MirrorWindow &m_window;
    ViewportMapper &m_mapper;
    AdbSession m_adb;
    MirrorConnection m_connection;
    std::unique_ptr<DecodeWorker> m_worker;
    QTimer m_resizeDebounce;
    QTimer m_keepalive;
    QElapsedTimer m_clock;
    KeyframeThrottle m_syncThrottle;
    int m_keyframeRequests = 0;
    QSize m_pendingView;
    qreal m_pendingRatio = 1.0;
    QSize m_requestedDisplay;
    int m_requestedDpi = 0;
    QSize m_streamSize;
    DecoderPath m_decoderPath = DecoderPath::Software;
    bool m_started = false;
    bool m_finished = false;
    bool m_waitingForClose = false;
    bool m_appExited = false;
    bool m_sessionReady = false;
    int m_exitCode = 0;
};

} // namespace qindaqt::link_mirror
