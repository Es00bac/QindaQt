// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/applet/task_list_applet_preview_port.h"

#include <QDBusConnection>
#include <QQueue>
#include <QTimer>
#include <QVariantMap>

#include <optional>

class QSocketNotifier;

namespace QindaQt::Shell {

// AGENT-CONTRACT (ADR-0241): The audited shell process alone may call KWin's
// restricted ScreenShot2 interface. This port captures only a named window,
// checks that ScreenShot2 and the QindaQt compositor have the same D-Bus
// owner, bounds the raw pipe payload, and delivers a small, owned QImage.
// Requests and results are GUI-thread confined. The caller owns the port and
// may cancel all pending work without receiving stale results.
class KWinScreenshotPreviewPort final
    : public ShellTaskListApplet::TaskListAppletPreviewPort {
    Q_OBJECT

public:
    explicit KWinScreenshotPreviewPort(
        QDBusConnection bus = QDBusConnection::sessionBus(),
        QObject *parent = nullptr);
    ~KWinScreenshotPreviewPort() override;

    void requestPreview(
        const ShellTaskListApplet::TaskListPreviewRequest &request) override;
    void cancelAll();

private:
    void startNext();
    void drainPipe();
    void receivedReply(quint64 serial, const QVariantMap &metadata,
                       const QString &error);
    void finishIfReady();
    void finish(bool ok, QImage image = {}, QString reason = {});
    void closePipe();

    QDBusConnection m_bus;
    QQueue<ShellTaskListApplet::TaskListPreviewRequest> m_queue;
    std::optional<ShellTaskListApplet::TaskListPreviewRequest> m_active;
    QSocketNotifier *m_notifier = nullptr;
    QTimer m_timeout;
    QVariantMap m_metadata;
    QByteArray m_bytes;
    QString m_kwinOwner;
    int m_readFd = -1;
    quint64 m_serial = 0;
    bool m_replyReceived = false;
    bool m_pipeEnded = false;
};

} // namespace QindaQt::Shell
