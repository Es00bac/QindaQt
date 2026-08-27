// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/session_lock_state/session_lock_state_monitor.h"

#include "qindaqt/services/session_lock_state/session_lock_transport.h"

#include <QRegularExpression>
#include <QThread>

#include <array>
#include <limits>
#include <utility>

namespace QindaQt::Services::SessionLockState {
namespace {

constexpr quint8 CompositorOwnerBit = 1U << 0U;
constexpr quint8 FreedesktopOwnerBit = 1U << 1U;
constexpr quint8 KdeOwnerBit = 1U << 2U;
constexpr quint8 CompleteOwnerMask =
    CompositorOwnerBit | FreedesktopOwnerBit | KdeOwnerBit;
constexpr qsizetype MaximumRetryCount = 16;
constexpr int MaximumRetryMilliseconds = 10'000;

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

quint8 serviceBit(ObservedService service)
{
    switch (service) {
    case ObservedService::Compositor:
        return CompositorOwnerBit;
    case ObservedService::FreedesktopScreenSaver:
        return FreedesktopOwnerBit;
    case ObservedService::KdeScreenSaver:
        return KdeOwnerBit;
    }
    return 0;
}

bool isPlausibleUniqueOwner(const QString &owner)
{
    // A unique D-Bus name begins with ':' and has at least two dot-separated
    // elements. The daemon is the source of this value; this extra validation
    // prevents a malformed fake/adapter reply becoming signal authority.
    static const QRegularExpression pattern(
        QStringLiteral(R"(^:[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)+$)"));
    return owner.size() <= 255 && pattern.match(owner).hasMatch();
}

bool validRetryPolicy(const SessionLockRetryPolicy &policy)
{
    if (policy.serviceObjectRetryMilliseconds.size() > MaximumRetryCount) {
        return false;
    }
    for (const int delay : policy.serviceObjectRetryMilliseconds) {
        if (delay <= 0 || delay > MaximumRetryMilliseconds) {
            return false;
        }
    }
    return true;
}

} // namespace

SessionLockStateMonitor::SessionLockStateMonitor(
    SessionLockTransport &transport, qint64 expectedKWinPid,
    SessionLockRetryPolicy retryPolicy, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_expectedKWinPid(expectedKWinPid)
    , m_retryPolicy(std::move(retryPolicy))
{
    connect(&m_transport, &SessionLockTransport::transportLost, this,
            &SessionLockStateMonitor::handleTransportLost);
    connect(&m_transport, &SessionLockTransport::serviceOwnerChanged, this,
            &SessionLockStateMonitor::handleServiceOwnerChanged);
    connect(&m_transport, &SessionLockTransport::serviceOwnerResolved, this,
            &SessionLockStateMonitor::handleServiceOwnerResolved);
    connect(&m_transport, &SessionLockTransport::unixProcessIdResolved, this,
            &SessionLockStateMonitor::handleUnixProcessIdResolved);
    connect(&m_transport, &SessionLockTransport::activeStateResolved, this,
            &SessionLockStateMonitor::handleActiveStateResolved);
    connect(&m_transport, &SessionLockTransport::requestFailed, this,
            &SessionLockStateMonitor::handleRequestFailed);
    connect(&m_transport, &SessionLockTransport::aboutToLock, this,
            &SessionLockStateMonitor::handleAboutToLock);
    connect(&m_transport, &SessionLockTransport::activeChanged, this,
            &SessionLockStateMonitor::handleActiveChanged);
    connect(&m_transport, &SessionLockTransport::activeRetryReady, this,
            &SessionLockStateMonitor::handleActiveRetryReady);
}

SessionLockStateMonitor::~SessionLockStateMonitor()
{
    stop();
}

LockState SessionLockStateMonitor::state() const noexcept
{
    return m_state;
}

bool SessionLockStateMonitor::contentMayBeShown() const noexcept
{
    return m_state == LockState::Unlocked;
}

bool SessionLockStateMonitor::isStarted() const noexcept
{
    return m_started;
}

bool SessionLockStateMonitor::start(QString *error)
{
    if (m_started) {
        setError(error, {});
        return true;
    }
    if (m_expectedKWinPid <= 0 ||
        static_cast<quint64>(m_expectedKWinPid) >
            std::numeric_limits<quint32>::max()) {
        setError(error, QStringLiteral("expected KWin PID is invalid"));
        return false;
    }
    if (!validRetryPolicy(m_retryPolicy)) {
        setError(error, QStringLiteral("session lock retry policy is invalid"));
        return false;
    }
    if (m_transport.thread() != thread()) {
        setError(error,
                 QStringLiteral("session lock transport has a different affinity thread"));
        return false;
    }
    QString transportError;
    if (!m_transport.start(&transportError)) {
        setError(error, transportError.trimmed().isEmpty()
                            ? QStringLiteral("session lock transport failed to start")
                            : transportError);
        return false;
    }

    m_started = true;
    beginAuthorityProbe();
    setError(error, {});
    return true;
}

void SessionLockStateMonitor::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    ++m_generation;
    invalidateSignalSerial();
    m_transport.unsubscribeFromLockSignals();
    m_transport.stop();
    m_compositorOwner.clear();
    m_freedesktopOwner.clear();
    m_kdeOwner.clear();
    m_authenticatedOwner.clear();
    m_ownerReplies = 0;
    m_pidReplyHandled = false;
    m_nextRetry = 0;
    setState(LockState::Unknown);
}

void SessionLockStateMonitor::handleTransportLost()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    ++m_generation;
    invalidateSignalSerial();
    m_transport.unsubscribeFromLockSignals();
    m_compositorOwner.clear();
    m_freedesktopOwner.clear();
    m_kdeOwner.clear();
    m_authenticatedOwner.clear();
    m_ownerReplies = 0;
    m_pidReplyHandled = false;
    m_nextRetry = 0;
    setState(LockState::Unknown);
}

void SessionLockStateMonitor::beginAuthorityProbe()
{
    if (!m_started) {
        return;
    }
    ++m_generation;
    invalidateSignalSerial();
    m_transport.unsubscribeFromLockSignals();
    m_compositorOwner.clear();
    m_freedesktopOwner.clear();
    m_kdeOwner.clear();
    m_authenticatedOwner.clear();
    m_ownerReplies = 0;
    m_pidReplyHandled = false;
    m_nextRetry = 0;
    setState(LockState::Unknown);

    // AGENT-CONTRACT: SessionLockTransport::start() installs all owner
    // watchers. These initial asynchronous queries therefore cannot create an
    // unobserved owner-change gap.
    constexpr std::array services{
        ObservedService::Compositor,
        ObservedService::FreedesktopScreenSaver,
        ObservedService::KdeScreenSaver,
    };
    for (const auto service : services) {
        m_transport.requestServiceOwner(m_generation, service);
    }
}

void SessionLockStateMonitor::handleServiceOwnerChanged(
    ObservedService service, const QString &uniqueOwner)
{
    Q_UNUSED(service)
    Q_UNUSED(uniqueOwner)
    if (m_started) {
        // Never trust the watcher payload as authority. It is only an
        // invalidation; the new generation queries all three names again.
        beginAuthorityProbe();
    }
}

void SessionLockStateMonitor::handleServiceOwnerResolved(
    quint64 generation, ObservedService service, const QString &uniqueOwner)
{
    if (!m_started || generation != m_generation) {
        return;
    }
    const quint8 bit = serviceBit(service);
    if (bit == 0 || (m_ownerReplies & bit) != 0) {
        return;
    }
    m_ownerReplies |= bit;
    if (!isPlausibleUniqueOwner(uniqueOwner)) {
        setState(LockState::Unknown);
        return;
    }

    switch (service) {
    case ObservedService::Compositor:
        m_compositorOwner = uniqueOwner;
        break;
    case ObservedService::FreedesktopScreenSaver:
        m_freedesktopOwner = uniqueOwner;
        break;
    case ObservedService::KdeScreenSaver:
        m_kdeOwner = uniqueOwner;
        break;
    }

    if (!hasCompleteOwnerQuorum()) {
        return;
    }
    const QString owner = commonOwner();
    if (owner.isEmpty()) {
        setState(LockState::Unknown);
        return;
    }
    m_transport.requestUnixProcessId(m_generation, owner);
}

void SessionLockStateMonitor::handleUnixProcessIdResolved(
    quint64 generation, const QString &uniqueOwner, quint64 processId)
{
    if (!m_started || generation != m_generation || m_pidReplyHandled ||
        uniqueOwner != commonOwner()) {
        return;
    }
    m_pidReplyHandled = true;
    if (processId == 0 ||
        processId != static_cast<quint64>(m_expectedKWinPid)) {
        setState(LockState::Unknown);
        return;
    }

    // AGENT-GUARD: Both exact-owner signal matches must exist before the first
    // GetActive request. Reversing this order can miss AboutToLock and publish
    // a stale false reply as permission to expose private notification data.
    if (!m_transport.subscribeToLockSignals(uniqueOwner)) {
        setState(LockState::Unknown);
        return;
    }
    m_authenticatedOwner = uniqueOwner;
    m_nextRetry = 0;
    m_confirmingInactive = false;
    issueActiveStateQuery();
}

void SessionLockStateMonitor::handleActiveStateResolved(
    quint64 generation, quint64 serial, const QString &uniqueOwner, bool active)
{
    if (!m_started || generation != m_generation || serial != m_signalSerial ||
        uniqueOwner != m_authenticatedOwner) {
        return;
    }

    if (active) {
        invalidateSignalSerial();
        setState(LockState::Locked);
        return;
    }
    if (m_confirmingInactive) {
        invalidateSignalSerial();
        setState(LockState::Unlocked);
        return;
    }

    // AGENT-GUARD: A single false query is never sufficient to expose public
    // content. A fresh serial forces a second asynchronous confirmation, while
    // AboutToLock or ActiveChanged fences both replies immediately.
    ++m_signalSerial;
    m_confirmingInactive = true;
    issueActiveStateQuery();
}

void SessionLockStateMonitor::handleRequestFailed(
    quint64 generation, quint64 serial, LockRequest request,
    ObservedService service, const QString &uniqueOwner,
    const QString &errorName, const QString &message)
{
    Q_UNUSED(service)
    Q_UNUSED(message)
    if (!m_started || generation != m_generation) {
        return;
    }
    if (request != LockRequest::ActiveState) {
        invalidateSignalSerial();
        m_transport.unsubscribeFromLockSignals();
        m_authenticatedOwner.clear();
        setState(LockState::Unknown);
        return;
    }
    if (serial != m_signalSerial || uniqueOwner != m_authenticatedOwner) {
        return;
    }

    invalidateSignalSerial();
    setState(LockState::Unknown);
    if (!retryableObjectError(errorName) ||
        m_nextRetry >= m_retryPolicy.serviceObjectRetryMilliseconds.size()) {
        return;
    }
    const int delay =
        m_retryPolicy.serviceObjectRetryMilliseconds.at(m_nextRetry++);
    m_transport.scheduleActiveRetry(m_generation, m_signalSerial, delay);
}

void SessionLockStateMonitor::handleAboutToLock(const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner != m_authenticatedOwner) {
        return;
    }
    invalidateSignalSerial();
    setState(LockState::Locking);
}

void SessionLockStateMonitor::handleActiveChanged(const QString &uniqueOwner,
                                                  bool active)
{
    if (!m_started || uniqueOwner != m_authenticatedOwner) {
        return;
    }
    invalidateSignalSerial();
    setState(active ? LockState::Locked : LockState::Unlocked);
}

void SessionLockStateMonitor::handleActiveRetryReady(quint64 generation,
                                                     quint64 serial)
{
    if (!m_started || generation != m_generation || serial != m_signalSerial ||
        m_authenticatedOwner.isEmpty()) {
        return;
    }
    issueActiveStateQuery();
}

void SessionLockStateMonitor::issueActiveStateQuery()
{
    if (!m_started || m_authenticatedOwner.isEmpty()) {
        return;
    }
    const quint64 serial = ++m_signalSerial;
    m_transport.requestActiveState(m_generation, serial,
                                   m_authenticatedOwner);
}

void SessionLockStateMonitor::setState(LockState state)
{
    if (m_state == state) {
        return;
    }
    const bool previousMayShow = contentMayBeShown();
    m_state = state;
    Q_EMIT stateChanged(m_state);
    const bool mayShow = contentMayBeShown();
    if (previousMayShow != mayShow) {
        Q_EMIT contentMayBeShownChanged(mayShow);
    }
}

void SessionLockStateMonitor::invalidateSignalSerial()
{
    ++m_signalSerial;
    m_confirmingInactive = false;
}

bool SessionLockStateMonitor::hasCompleteOwnerQuorum() const noexcept
{
    return m_ownerReplies == CompleteOwnerMask &&
           !m_compositorOwner.isEmpty() && !m_freedesktopOwner.isEmpty() &&
           !m_kdeOwner.isEmpty();
}

QString SessionLockStateMonitor::commonOwner() const
{
    if (!hasCompleteOwnerQuorum() ||
        m_compositorOwner != m_freedesktopOwner ||
        m_compositorOwner != m_kdeOwner) {
        return {};
    }
    return m_compositorOwner;
}

bool SessionLockStateMonitor::retryableObjectError(
    const QString &errorName) const
{
    return errorName == QLatin1StringView("org.freedesktop.DBus.Error.UnknownObject") ||
           errorName == QLatin1StringView("org.freedesktop.DBus.Error.UnknownMethod");
}

} // namespace QindaQt::Services::SessionLockState
