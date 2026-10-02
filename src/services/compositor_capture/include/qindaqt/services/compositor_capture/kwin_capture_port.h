// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/compositor_capture/capture_port.h>

#include <QByteArray>
#include <QDeadlineTimer>
#include <QDBusConnection>
#include <QTimer>
#include <QVariantMap>

class QSocketNotifier;

namespace QindaQt::CompositorCapture {

// Captures through KWin's restricted org.qindaqt.KWin.ScreenShot2 interface.
//
// AGENT-CONTRACT (ADR-0289/0324): permission belongs to the compositor's
// ordinary desktop-entry or authenticated protected-job lease, never this
// transport. Native and compatibility screenshot owners must agree. Decode
// only after complete reply and pipe EOF within the call's original total.
// Protected consumers set30s pending timeout/zero grace; legacy builders keep
// their existing timeout plus10s grace, including interactive selection.
class KWinCapturePort final : public CapturePort {
    Q_OBJECT

public:
    explicit KWinCapturePort(QDBusConnection bus = QDBusConnection::sessionBus(),
                             QObject *parent = nullptr);
    ~KWinCapturePort() override;

    bool capture(const KWinCaptureCall &call) override;
    void cancel() override;
    [[nodiscard]] bool busy() const override { return m_active; }

private:
    void drainPipe();
    void receivedReply(quint64 serial, const QVariantMap &metadata, const QString &errorName,
                       const QString &errorMessage);
    void finishIfReady();
    void finish(DecodedCapture result);
    void closePipe();
    void reset();

    QDBusConnection m_bus;
    QSocketNotifier *m_notifier = nullptr;
    QTimer m_timeout;
    QDeadlineTimer m_deadline;
    QVariantMap m_metadata;
    QByteArray m_bytes;
    QString m_kwinOwner;
    int m_readFd = -1;
    quint64 m_serial = 0;
    bool m_active = false;
    bool m_replyReceived = false;
    bool m_pipeEnded = false;
};

} // namespace QindaQt::CompositorCapture
