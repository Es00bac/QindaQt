// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_visibility_client/compositor_visibility_client.h"

#include "qindaqt/shell_visibility_client/compositor_visibility_transport.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::ShellVisibilityClient {
namespace {

constexpr int MaximumRuntimeDelayMilliseconds = 60'000;

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

} // namespace

bool CompositorVisibilityClientTiming::isValid() const noexcept
{
    if (debounceMilliseconds < 0 ||
        debounceMilliseconds > MaximumRuntimeDelayMilliseconds ||
        requestTimeoutMilliseconds <= 0 ||
        requestTimeoutMilliseconds > MaximumRuntimeDelayMilliseconds ||
        retryMilliseconds.isEmpty()) {
        return false;
    }
    int previous = 0;
    for (const int delay : retryMilliseconds) {
        if (delay <= 0 || delay > MaximumRuntimeDelayMilliseconds || delay < previous) {
            return false;
        }
        previous = delay;
    }
    return true;
}

CompositorVisibilityClient::CompositorVisibilityClient(
    CompositorVisibilityTransport &transport,
    CompositorVisibilityClientTiming timing,
    QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_timing(std::move(timing))
{
    m_refreshTimer.setSingleShot(true);
    m_requestTimeout.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout,
            this, &CompositorVisibilityClient::handleRefreshTimer);
    connect(&m_requestTimeout, &QTimer::timeout,
            this, &CompositorVisibilityClient::handleRequestTimeout);
    connect(&m_transport, &CompositorVisibilityTransport::serviceOwnerChanged,
            this, &CompositorVisibilityClient::handleServiceOwnerChanged);
    connect(&m_transport, &CompositorVisibilityTransport::snapshotInvalidated,
            this, &CompositorVisibilityClient::handleInvalidation);
    connect(&m_transport, &CompositorVisibilityTransport::snapshotReceived,
            this, &CompositorVisibilityClient::handleSnapshot);
    connect(&m_transport, &CompositorVisibilityTransport::snapshotFailed,
            this, &CompositorVisibilityClient::handleFailure);
}

CompositorVisibilityClient::~CompositorVisibilityClient()
{
    stop();
}

bool CompositorVisibilityClient::start(QString *error)
{
    if (m_started) {
        if (error) {
            error->clear();
        }
        return true;
    }
    if (!m_timing.isValid()) {
        setError(error, QStringLiteral("compositor visibility client timing is invalid"));
        return false;
    }
    m_started = true;
    if (!m_transport.start(error)) {
        m_started = false;
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

void CompositorVisibilityClient::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    m_refreshTimer.stop();
    m_requestTimeout.stop();
    m_timerPurpose = TimerPurpose::None;
    m_inFlight.reset();
    m_dirty = false;
    m_transport.stop();
}

const std::optional<ShellVisibility::CompositorVisibilitySnapshot> &
CompositorVisibilityClient::snapshot() const noexcept
{
    return m_state.snapshot();
}

bool CompositorVisibilityClient::safeVisibleRequired() const noexcept
{
    return m_state.safeVisibleRequired();
}

const ShellVisibility::CompositorVisibilityStateResult &
CompositorVisibilityClient::lastResult() const noexcept
{
    return m_lastResult;
}

bool CompositorVisibilityClient::requestInFlight() const noexcept
{
    return m_inFlight.has_value();
}

void CompositorVisibilityClient::handleServiceOwnerChanged(
    const QString &uniqueOwner)
{
    if (!m_started) {
        return;
    }
    m_refreshTimer.stop();
    m_requestTimeout.stop();
    m_timerPurpose = TimerPurpose::None;
    m_inFlight.reset();
    m_dirty = false;
    m_retryIndex = 0;

    if (uniqueOwner.isEmpty()) {
        const auto current = m_state.currentRequestTag();
        if (current) {
            publishResult(m_state.serviceLost(*current));
        }
        return;
    }

    const auto result = m_state.observeServiceOwner(uniqueOwner);
    const bool accepted = result.code ==
        ShellVisibility::CompositorVisibilityStateErrorCode::None;
    publishResult(result);
    if (accepted) {
        scheduleDebounce(0);
    }
}

void CompositorVisibilityClient::handleInvalidation(const QString &uniqueOwner)
{
    if (!m_started || !currentOwnerIs(uniqueOwner)) {
        return;
    }
    if (m_inFlight) {
        m_dirty = true;
        return;
    }
    scheduleDebounce(m_timing.debounceMilliseconds);
}

void CompositorVisibilityClient::handleSnapshot(
    quint64 token, const QString &uniqueOwner, const QByteArray &payload)
{
    if (!m_started || !m_inFlight || m_inFlight->token != token ||
        m_inFlight->owner.uniqueOwner != uniqueOwner) {
        return;
    }
    const auto request = m_inFlight->owner;
    m_inFlight.reset();
    m_requestTimeout.stop();
    completeRequest(m_state.acceptSnapshot(request, payload), true);
}

void CompositorVisibilityClient::handleFailure(
    quint64 token, const QString &uniqueOwner, const QString &message)
{
    if (!m_started || !m_inFlight || m_inFlight->token != token ||
        m_inFlight->owner.uniqueOwner != uniqueOwner) {
        return;
    }
    const auto request = m_inFlight->owner;
    m_inFlight.reset();
    m_requestTimeout.stop();
    completeRequest(m_state.requestFailed(request, message), true);
}

void CompositorVisibilityClient::handleRefreshTimer()
{
    m_timerPurpose = TimerPurpose::None;
    requestNow();
}

void CompositorVisibilityClient::handleRequestTimeout()
{
    if (!m_started || !m_inFlight) {
        return;
    }
    const auto request = m_inFlight->owner;
    m_inFlight.reset();
    completeRequest(
        m_state.requestFailed(request,
                              QStringLiteral("compositor snapshot request timed out")),
        true);
}

void CompositorVisibilityClient::scheduleDebounce(int milliseconds)
{
    if (!m_started || m_inFlight || !m_state.currentRequestTag()) {
        return;
    }
    if (m_refreshTimer.isActive() && m_timerPurpose == TimerPurpose::Debounce) {
        return;
    }
    m_refreshTimer.stop();
    m_timerPurpose = TimerPurpose::Debounce;
    m_refreshTimer.start(milliseconds);
}

void CompositorVisibilityClient::scheduleRetry()
{
    if (!m_started || m_inFlight || !m_state.currentRequestTag() ||
        m_timing.retryMilliseconds.isEmpty()) {
        return;
    }
    const qsizetype last = m_timing.retryMilliseconds.size() - 1;
    const qsizetype index = std::min(m_retryIndex, last);
    const int delay = m_timing.retryMilliseconds.at(index);
    if (m_retryIndex < last) {
        ++m_retryIndex;
    }
    m_refreshTimer.stop();
    m_timerPurpose = TimerPurpose::Retry;
    m_refreshTimer.start(delay);
}

void CompositorVisibilityClient::requestNow()
{
    if (!m_started || m_inFlight) {
        return;
    }
    const auto owner = m_state.currentRequestTag();
    if (!owner) {
        return;
    }
    if (m_nextToken == 0) {
        completeRequest(
            m_state.requestFailed(*owner,
                                  QStringLiteral("snapshot request token is exhausted")),
            false);
        return;
    }
    const quint64 token = m_nextToken;
    m_nextToken = token == std::numeric_limits<quint64>::max() ? 0 : token + 1;
    m_inFlight = InFlightRequest{token, *owner};
    m_requestTimeout.start(m_timing.requestTimeoutMilliseconds);
    m_transport.requestSnapshot(token, owner->uniqueOwner);
}

void CompositorVisibilityClient::completeRequest(
    ShellVisibility::CompositorVisibilityStateResult result,
    bool permitRetry)
{
    const bool followUp = m_dirty;
    m_dirty = false;
    const bool accepted = result.ok();
    const bool stale = result.stale();
    publishResult(std::move(result));
    if (!m_started) {
        return;
    }
    if (accepted) {
        m_retryIndex = 0;
        if (followUp) {
            scheduleDebounce(m_timing.debounceMilliseconds);
        }
        return;
    }
    if (stale || !permitRetry) {
        return;
    }
    if (followUp) {
        scheduleDebounce(m_timing.debounceMilliseconds);
    } else {
        scheduleRetry();
    }
}

void CompositorVisibilityClient::publishResult(
    ShellVisibility::CompositorVisibilityStateResult result)
{
    const bool changed = result.stateChanged;
    m_lastResult = std::move(result);
    if (changed) {
        Q_EMIT stateChanged();
    }
}

bool CompositorVisibilityClient::currentOwnerIs(
    const QString &uniqueOwner) const
{
    const auto owner = m_state.currentRequestTag();
    return owner && owner->uniqueOwner == uniqueOwner;
}

} // namespace QindaQt::ShellVisibilityClient
