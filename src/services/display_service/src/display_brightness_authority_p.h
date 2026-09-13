// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_model.h>

#include <optional>

namespace QindaQt::DisplayService::Private
{

// AGENT-CONTRACT: The D7 immediate brightness authority owns the sibling
// BrightnessSnapshot publication and at most one pending request. It never
// drives the D1 machine: every call receives the accepted topology snapshot
// (nullptr while unavailable), and admission receives machine state and
// safety. A pending request finishes exactly once, is never replayed, and
// drains through takeFinishes(). The clock and port are borrowed from the
// owning model and must outlive this object.
class BrightnessAuthority final
{
public:
    BrightnessAuthority(DisplayTransaction::MonotonicClock &clock, TransactionPort &port,
                        quint64 deadlineMilliseconds);

    [[nodiscard]] const Display::BrightnessSnapshot *snapshot() const noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] quint64 deadlineMonotonicMilliseconds() const noexcept;

    void refresh(const Display::Snapshot *topology);
    void devicesObserved(const DeviceBrightnessFrame &frame,
                         const Display::Snapshot *topology);
    [[nodiscard]] BrightnessRequestResult request(
        const Display::BrightnessRequest &request, const Display::Snapshot *topology,
        DisplayTransaction::MachineState state, DisplayTransaction::SafetyState safety);
    void completed(quint64 requestId, BrightnessApplyOutcome outcome);
    void tick();

    [[nodiscard]] QList<BrightnessFinish> takeFinishes();
    [[nodiscard]] bool takePublicationChanged();

private:
    struct Pending {
        quint64 requestId = 0;
        QString stableId;
        QString epoch;
        quint64 initiatingRevision = 0;
        quint64 ownerGeneration = 0;
        quint32 value = 0;
        quint64 deadline = 0;
        bool acknowledged = false;
    };

    void republish(const Display::Snapshot *topology);
    void resolvePending();
    void finish(Display::OperationStatus status, Display::ErrorCode error,
                const QString &diagnostic);
    [[nodiscard]] const DeviceBrightness *joinedDevice(const Display::Output &output) const;
    [[nodiscard]] BrightnessRequestResult finalResult(Display::OperationStatus status,
                                                      Display::ErrorCode error,
                                                      const QString &diagnostic) const;

    DisplayTransaction::MonotonicClock &m_clock;
    TransactionPort &m_port;
    quint64 m_deadlineMilliseconds = 1;
    DeviceBrightnessFrame m_devices;
    std::optional<Display::BrightnessSnapshot> m_published;
    // Brightness revision at which the current topology revision was joined.
    // A request pinned below it names a replaced output identity.
    quint64 m_joinRevision = 0;
    QString m_exhaustedEpoch;
    std::optional<Pending> m_pending;
    QList<BrightnessFinish> m_finishes;
    quint64 m_nextRequestId = 1;
    bool m_publicationChanged = false;
};

} // namespace QindaQt::DisplayService::Private
