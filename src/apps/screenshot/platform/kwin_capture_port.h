// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "capture_port.h"

#include <QByteArray>
#include <QDBusConnection>
#include <QTimer>
#include <QVariantMap>

class QSocketNotifier;

namespace QindaQt::Screenshot {

// Captures through KWin's restricted org.kde.KWin.ScreenShot2 interface.
//
// AGENT-CONTRACT (ADR-0289, following ADR-0241): KWin admits this process
// only because the installed org.qindaqt.Screenshot desktop entry names the
// exact executable and lists org.kde.KWin.ScreenShot2 in
// X-KDE-DBUS-Restricted-Interfaces. The port refuses to talk to a
// ScreenShot2 owner that is not also org.kde.KWin, reads the raw image from
// a pipe it creates, and decodes it only after both the reply and pipe EOF.
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
    QVariantMap m_metadata;
    QByteArray m_bytes;
    QString m_kwinOwner;
    int m_readFd = -1;
    quint64 m_serial = 0;
    bool m_active = false;
    bool m_replyReceived = false;
    bool m_pipeEnded = false;
};

} // namespace QindaQt::Screenshot
