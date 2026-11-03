// SPDX-License-Identifier: GPL-3.0-or-later
#include "decode_worker.h"

#include <QMetaObject>
#include <QMutexLocker>

#include <utility>

namespace qindaqt::link_mirror {

DecodeWorker::DecodeWorker(DecoderPreference preference, DecoderFactory factory)
    : m_selector(preference, std::move(factory))
{
    m_thread.setObjectName(QStringLiteral("qindalink-mirror-decode"));
    m_context.moveToThread(&m_thread);
    m_thread.start();
}

DecodeWorker::~DecodeWorker()
{
    m_thread.quit();
    m_thread.wait();
}

void DecodeWorker::postFormat(const VideoFormat &format)
{
    m_gate.reset();
    QMetaObject::invokeMethod(&m_context, [this, format] { handleFormat(format); }, Qt::QueuedConnection);
}

void DecodeWorker::postPacket(const VideoPacket &packet)
{
    switch (m_gate.admit(packet, m_inFlight.load())) {
    case BacklogGate::Verdict::Drop:
        ++m_dropped;
        return;
    case BacklogGate::Verdict::DropAndRequestKeyframe:
        ++m_dropped;
        Q_EMIT keyframeNeeded();
        return;
    case BacklogGate::Verdict::Queue:
        break;
    }
    m_inFlight.fetch_add(1);
    QMetaObject::invokeMethod(
        &m_context,
        [this, packet] {
            handlePacket(packet);
            m_inFlight.fetch_sub(1);
        },
        Qt::QueuedConnection);
}

QImage DecodeWorker::takeFrame()
{
    QMutexLocker lock(&m_mailboxLock);
    return std::exchange(m_mailbox, QImage());
}

void DecodeWorker::handleFormat(const VideoFormat &format)
{
    m_waitingForKey = true;
    if (!m_selector.open(format)) {
        Q_EMIT decoderFailed(m_selector.lastError());
        return;
    }
    if (!m_selector.lastError().isEmpty()) {
        Q_EMIT decodeWarning(m_selector.lastError());
    }
    Q_EMIT decoderOpened(m_selector.activePath(), format.size);
}

void DecodeWorker::handlePacket(const VideoPacket &packet)
{
    if (!m_selector.isOpen()) {
        return;
    }
    // A decoder that starts mid-GOP only produces garbage until the next keyframe; skip to it.
    if (m_waitingForKey && !packet.config && !packet.key) {
        return;
    }
    if (packet.key) {
        m_waitingForKey = false;
    }
    QImage image;
    const DecodeResult result = m_selector.decode(packet, &image);
    if (result == DecodeResult::Error) {
        Q_EMIT decodeWarning(m_selector.lastError());
        if (m_selector.takeKeyframeRequest()) {
            m_waitingForKey = true;
            Q_EMIT decoderOpened(m_selector.activePath(), {});
            Q_EMIT keyframeNeeded();
        }
        return;
    }
    if (result != DecodeResult::Frame) {
        return;
    }
    m_decoded.fetch_add(1);
    bool wasEmpty = false;
    {
        QMutexLocker lock(&m_mailboxLock);
        wasEmpty = m_mailbox.isNull();
        m_mailbox = std::move(image);
    }
    if (wasEmpty) {
        Q_EMIT frameAvailable();
    }
}

} // namespace qindaqt::link_mirror
