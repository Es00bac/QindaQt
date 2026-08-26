// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility/compositor_visibility_state.h"

#include <QObject>
#include <QTimer>
#include <QVector>

#include <optional>

namespace QindaQt::ShellVisibilityClient {

class CompositorVisibilityTransport;

struct CompositorVisibilityClientTiming {
    int debounceMilliseconds = 16;
    int requestTimeoutMilliseconds = 2000;
    QVector<int> retryMilliseconds = {100, 250, 500, 1000, 2000, 5000};

    [[nodiscard]] bool isValid() const noexcept;
};

// Owns coalescing, timeout, retry, and owner-lineage policy. It never knows
// QDBus types and can therefore be qualified with a deterministic fake.
class CompositorVisibilityClient final : public QObject {
    Q_OBJECT

public:
    explicit CompositorVisibilityClient(
        CompositorVisibilityTransport &transport,
        CompositorVisibilityClientTiming timing = {},
        QObject *parent = nullptr);
    ~CompositorVisibilityClient() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();

    [[nodiscard]] const std::optional<ShellVisibility::CompositorVisibilitySnapshot> &
    snapshot() const noexcept;
    [[nodiscard]] bool safeVisibleRequired() const noexcept;
    [[nodiscard]] const ShellVisibility::CompositorVisibilityStateResult &
    lastResult() const noexcept;
    [[nodiscard]] bool requestInFlight() const noexcept;

Q_SIGNALS:
    void stateChanged();

private Q_SLOTS:
    void handleServiceOwnerChanged(const QString &uniqueOwner);
    void handleInvalidation(const QString &uniqueOwner);
    void handleSnapshot(quint64 token, const QString &uniqueOwner,
                        const QByteArray &payload);
    void handleFailure(quint64 token, const QString &uniqueOwner,
                       const QString &message);
    void handleRefreshTimer();
    void handleRequestTimeout();

private:
    enum class TimerPurpose {
        None,
        Debounce,
        Retry,
    };

    struct InFlightRequest {
        quint64 token = 0;
        ShellVisibility::CompositorVisibilityRequestTag owner;
    };

    void scheduleDebounce(int milliseconds);
    void scheduleRetry();
    void requestNow();
    void completeRequest(
        ShellVisibility::CompositorVisibilityStateResult result,
        bool permitRetry);
    void publishResult(ShellVisibility::CompositorVisibilityStateResult result);
    [[nodiscard]] bool currentOwnerIs(const QString &uniqueOwner) const;

    CompositorVisibilityTransport &m_transport;
    CompositorVisibilityClientTiming m_timing;
    ShellVisibility::CompositorVisibilitySnapshotStateMachine m_state;
    ShellVisibility::CompositorVisibilityStateResult m_lastResult;
    QTimer m_refreshTimer;
    QTimer m_requestTimeout;
    std::optional<InFlightRequest> m_inFlight;
    TimerPurpose m_timerPurpose = TimerPurpose::None;
    qsizetype m_retryIndex = 0;
    quint64 m_nextToken = 1;
    bool m_dirty = false;
    bool m_started = false;
};

} // namespace QindaQt::ShellVisibilityClient
