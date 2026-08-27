// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/session_lock_state/session_lock_transport.h"

#include <QStringList>
#include <QVector>

namespace QindaQt::Services::SessionLockState::TestSupport {

struct OwnerRequestRecord {
    quint64 generation = 0;
    ObservedService service = ObservedService::Compositor;
};

struct ProcessIdRequestRecord {
    quint64 generation = 0;
    QString owner;
};

struct ActiveRequestRecord {
    quint64 generation = 0;
    quint64 serial = 0;
    QString owner;
};

struct RetryRecord {
    quint64 generation = 0;
    quint64 serial = 0;
    int delayMilliseconds = 0;
};

class FakeSessionLockTransport final : public SessionLockTransport {
public:
    using SessionLockTransport::SessionLockTransport;

    bool start(QString *error) override
    {
        ++startCount;
        events.append(QStringLiteral("start"));
        if (!startAccepted) {
            if (error != nullptr) {
                *error = QStringLiteral("injected start failure");
            }
            return false;
        }
        started = true;
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }

    void stop() override
    {
        ++stopCount;
        started = false;
        events.append(QStringLiteral("stop"));
    }

    void requestServiceOwner(quint64 generation,
                             ObservedService service) override
    {
        ownerRequests.append({generation, service});
        events.append(QStringLiteral("owner:%1").arg(int(service)));
    }

    void requestUnixProcessId(quint64 generation,
                              const QString &uniqueOwner) override
    {
        processIdRequests.append({generation, uniqueOwner});
        events.append(QStringLiteral("pid"));
    }

    bool subscribeToLockSignals(const QString &uniqueOwner) override
    {
        subscribedOwner = uniqueOwner;
        events.append(QStringLiteral("subscribe"));
        return subscriptionAccepted;
    }

    void unsubscribeFromLockSignals() override
    {
        subscribedOwner.clear();
        events.append(QStringLiteral("unsubscribe"));
    }

    void requestActiveState(quint64 generation, quint64 serial,
                            const QString &uniqueOwner) override
    {
        activeRequests.append({generation, serial, uniqueOwner});
        events.append(QStringLiteral("active"));
    }

    void scheduleActiveRetry(quint64 generation, quint64 serial,
                             int delayMilliseconds) override
    {
        retries.append({generation, serial, delayMilliseconds});
        events.append(QStringLiteral("retry"));
    }

    void resolveOwner(const OwnerRequestRecord &request, const QString &owner)
    {
        Q_EMIT serviceOwnerResolved(request.generation, request.service, owner);
    }

    void resolvePid(const ProcessIdRequestRecord &request, quint64 pid)
    {
        Q_EMIT unixProcessIdResolved(request.generation, request.owner, pid);
    }

    void resolveActive(const ActiveRequestRecord &request, bool active)
    {
        Q_EMIT activeStateResolved(request.generation, request.serial,
                                   request.owner, active);
    }

    void fail(const ActiveRequestRecord &request, const QString &errorName)
    {
        Q_EMIT requestFailed(
            request.generation, request.serial, LockRequest::ActiveState,
            ObservedService::FreedesktopScreenSaver, request.owner, errorName,
            QStringLiteral("injected failure"));
    }

    void fireRetry(const RetryRecord &retry)
    {
        Q_EMIT activeRetryReady(retry.generation, retry.serial);
    }

    void loseTransport()
    {
        started = false;
        Q_EMIT transportLost();
    }

    bool startAccepted = true;
    bool subscriptionAccepted = true;
    bool started = false;
    int startCount = 0;
    int stopCount = 0;
    QString subscribedOwner;
    QStringList events;
    QVector<OwnerRequestRecord> ownerRequests;
    QVector<ProcessIdRequestRecord> processIdRequests;
    QVector<ActiveRequestRecord> activeRequests;
    QVector<RetryRecord> retries;
};

inline void resolveMatchingOwners(FakeSessionLockTransport &transport,
                                  const QString &owner = QStringLiteral(":1.42"))
{
    const auto requests = transport.ownerRequests;
    for (const auto &request : requests) {
        transport.resolveOwner(request, owner);
    }
}

inline ActiveRequestRecord authenticate(
    FakeSessionLockTransport &transport, quint64 expectedPid,
    const QString &owner = QStringLiteral(":1.42"))
{
    resolveMatchingOwners(transport, owner);
    transport.resolvePid(transport.processIdRequests.constLast(), expectedPid);
    return transport.activeRequests.constLast();
}

} // namespace QindaQt::Services::SessionLockState::TestSupport
