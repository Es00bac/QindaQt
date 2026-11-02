// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/link_mirror/decoder_selection.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QThread>

#include <atomic>

// Runs a DecoderSelector on its own thread and hands the newest picture to the GUI thread.
//
// Ownership: DecodeWorker owns its QThread and the selector; destroy it from the GUI thread.
// Threading: post* methods may be called from the GUI thread; signals are emitted on the
// worker thread and must be connected with queued or auto connections.
//
// AGENT-NOTE: the hand-off is a one-slot mailbox (newest frame wins). If presentation falls
// behind, old pictures are dropped instead of queueing, so latency never accumulates.
namespace qindaqt::link_mirror {

class DecodeWorker : public QObject {
    Q_OBJECT

public:
    DecodeWorker(DecoderPreference preference, DecoderFactory factory);
    ~DecodeWorker() override;

    void postFormat(const VideoFormat &format);
    void postPacket(const VideoPacket &packet);

    // Takes the newest decoded picture; null when none arrived since the last call.
    [[nodiscard]] QImage takeFrame();
    [[nodiscard]] quint64 decodedFrames() const { return m_decoded.load(); }

Q_SIGNALS:
    void frameAvailable();
    void decoderOpened(qindaqt::link_mirror::DecoderPath path, QSize size);
    void keyframeNeeded();
    void decoderFailed(const QString &reason);
    void decodeWarning(const QString &reason);

private:
    void handleFormat(const VideoFormat &format);
    void handlePacket(const VideoPacket &packet);

    QThread m_thread;
    QObject m_context; // lives on m_thread; queued calls run there
    DecoderSelector m_selector;
    QMutex m_mailboxLock;
    QImage m_mailbox;
    std::atomic<quint64> m_decoded{0};
    bool m_waitingForKey = true;
};

} // namespace qindaqt::link_mirror
