// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/session_lock_state/session_lock_state.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QtTypes>

namespace QindaQt::Services::SessionLockState {

class SessionLockTransport;

struct SessionLockRetryPolicy {
    QList<int> serviceObjectRetryMilliseconds{50, 100, 250, 500, 1'000};
};

class SessionLockStateMonitor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QindaQt::Services::SessionLockState::LockState state READ state
                   NOTIFY stateChanged)
    Q_PROPERTY(bool contentMayBeShown READ contentMayBeShown
                   NOTIFY contentMayBeShownChanged)

public:
    // The transport is borrowed and must outlive the monitor. Both objects are
    // thread-confined to the same affinity thread. expectedKWinPid is supplied
    // by the session supervisor; zero, negative, or non-Linux-PID-sized values
    // make start() fail while the public state remains fail-closed Unknown.
    explicit SessionLockStateMonitor(
        SessionLockTransport &transport, qint64 expectedKWinPid,
        SessionLockRetryPolicy retryPolicy = {}, QObject *parent = nullptr);
    ~SessionLockStateMonitor() override;

    [[nodiscard]] LockState state() const noexcept;
    [[nodiscard]] bool contentMayBeShown() const noexcept;
    [[nodiscard]] bool isStarted() const noexcept;

    // Startup installs owner watchers before issuing any asynchronous owner
    // query. Errors are returned synchronously only for invalid local setup;
    // all remote failures are represented by Unknown state.
    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();

Q_SIGNALS:
    void stateChanged(QindaQt::Services::SessionLockState::LockState state);
    void contentMayBeShownChanged(bool contentMayBeShown);

private:
    void beginAuthorityProbe();
    void handleTransportLost();
    void handleServiceOwnerChanged(ObservedService service,
                                   const QString &uniqueOwner);
    void handleServiceOwnerResolved(quint64 generation,
                                    ObservedService service,
                                    const QString &uniqueOwner);
    void handleUnixProcessIdResolved(quint64 generation,
                                     const QString &uniqueOwner,
                                     quint64 processId);
    void handleActiveStateResolved(quint64 generation, quint64 serial,
                                   const QString &uniqueOwner, bool active);
    void handleRequestFailed(quint64 generation, quint64 serial,
                             LockRequest request, ObservedService service,
                             const QString &uniqueOwner,
                             const QString &errorName,
                             const QString &message);
    void handleAboutToLock(const QString &uniqueOwner);
    void handleActiveChanged(const QString &uniqueOwner, bool active);
    void handleActiveRetryReady(quint64 generation, quint64 serial);
    void issueActiveStateQuery();
    void setState(LockState state);
    void invalidateSignalSerial();
    [[nodiscard]] bool hasCompleteOwnerQuorum() const noexcept;
    [[nodiscard]] QString commonOwner() const;
    [[nodiscard]] bool retryableObjectError(const QString &errorName) const;

    SessionLockTransport &m_transport;
    qint64 m_expectedKWinPid = 0;
    SessionLockRetryPolicy m_retryPolicy;
    QString m_compositorOwner;
    QString m_freedesktopOwner;
    QString m_kdeOwner;
    QString m_authenticatedOwner;
    LockState m_state = LockState::Unknown;
    quint8 m_ownerReplies = 0;
    quint64 m_generation = 0;
    quint64 m_signalSerial = 0;
    qsizetype m_nextRetry = 0;
    bool m_pidReplyHandled = false;
    bool m_confirmingInactive = false;
    bool m_started = false;
};

} // namespace QindaQt::Services::SessionLockState
