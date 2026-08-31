// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositoroutputauthority.h"

#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QTimer>

#include <optional>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::Shell {

// GUI-thread production adapter for the public Compositor1 Outputs projection.
// It owns no compositor or screen objects. Frames are immutable value copies,
// owner-bound, invalidated before refresh, and borrowed only until stateChanged.
class QtCompositorOutputAuthority final : public QObject {
    Q_OBJECT

public:
    explicit QtCompositorOutputAuthority(
        QDBusConnection connection = QDBusConnection::sessionBus(),
        QObject *parent = nullptr);
    ~QtCompositorOutputAuthority() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    [[nodiscard]] const std::optional<CompositorOutputAuthorityFrame> &frame()
        const noexcept;

Q_SIGNALS:
    void stateChanged();

private Q_SLOTS:
    void outputsChanged();

private:
    void resolveInitialOwner();
    void bindOwner(const QString &uniqueOwner);
    void requestSnapshot();
    void scheduleRetry();
    void publish(std::optional<CompositorOutputAuthorityFrame> frame,
                 bool notify = true);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QList<QDBusPendingCallWatcher *> m_pendingCalls;
    QTimer m_retryTimer;
    std::optional<CompositorOutputAuthorityFrame> m_frame;
    QString m_uniqueOwner;
    quint64 m_resolutionGeneration = 0;
    quint64 m_requestSerial = 0;
    qsizetype m_retryIndex = 0;
    bool m_started = false;
    bool m_inFlight = false;
    bool m_dirty = false;
};

} // namespace QindaQt::Shell
