// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_ports.h>
#include <qindaqt/services/display_writer/output_management_port.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <memory>
#include <optional>

namespace QindaQt::DisplayWriter
{

class JournalStore
{
public:
    virtual ~JournalStore() = default;
    [[nodiscard]] virtual DisplayTransaction::JournalMutationOutcome store(
        const DisplayTransaction::Journal &journal) = 0;
    [[nodiscard]] virtual DisplayTransaction::JournalMutationOutcome clear() = 0;
};

class WriterTransactionPort final : public QObject,
                                    public DisplayService::TransactionPort,
                                    private OutputManagementObserver
{
    Q_OBJECT

public:
    // Takes exclusive ownership of both narrow side-effect adapters. All calls,
    // timer transitions, and callbacks remain on the constructing Qt thread.
    // A timeout is clamped to at least 1 ms. The DisplayService observer is
    // borrowed and may be detached before destruction.
    WriterTransactionPort(std::unique_ptr<OutputManagementPort> outputManagement,
                          std::unique_ptr<JournalStore> journalStore,
                          quint64 requestTimeoutMilliseconds = 5'000,
                          QObject *parent = nullptr);
    ~WriterTransactionPort() override;

    [[nodiscard]] PortStartStatus start();
    void stop();
    [[nodiscard]] bool isStarted() const noexcept;
    [[nodiscard]] bool isOutputManagementAvailable() const noexcept;
    [[nodiscard]] qint64 compositorProcessId() const noexcept;

    void setObserver(DisplayService::TransactionPortObserver *observer) override;
    void beginMachineLineage(quint64 machineLineage) override;
    [[nodiscard]] DisplayTransaction::JournalMutationOutcome storeJournal(
        const DisplayTransaction::Journal &journal) override;
    [[nodiscard]] DisplayTransaction::JournalMutationOutcome clearJournal() override;
    void requestApply(const DisplayTransaction::ApplyRequest &request) override;
    // Fenced to the exact owner generation of the forwarded device frame and
    // mutually exclusive with a topology apply. Completion and device frames
    // reach the observer on a later event-loop turn, in arrival order.
    [[nodiscard]] DisplayService::BrightnessSubmitStatus requestBrightness(
        const DisplayService::BrightnessApplyRequest &request) override;

Q_SIGNALS:
    // D6 combines this edge with authenticated lock and logind authority; it
    // is not, by itself, permission for a preview.
    void mutationAuthorityChanged(bool available);

private:
    struct Pending {
        quint64 machineLineage = 0;
        quint64 token = 0;
        quint64 requestId = 0;
        quint64 ownerGeneration = 0;
    };

    struct BrightnessPending {
        quint64 serviceRequestId = 0;
        quint64 requestId = 0;
        quint64 ownerGeneration = 0;
    };

    void outputManagementOwnerChanged(quint64 ownerGeneration,
                                      bool available) override;
    void outputManagementCompleted(quint64 ownerGeneration,
                                   quint64 requestId,
                                   CompletionOutcome outcome) override;
    void outputManagementDevicesObserved(
        quint64 ownerGeneration, const QList<OutputDeviceState> &devices) override;
    void finishPending(DisplayTransaction::ApplyOutcome outcome);
    void finishBrightness(DisplayService::BrightnessApplyOutcome outcome);
    void finishDeferred(quint64 machineLineage, quint64 token,
                        DisplayTransaction::ApplyOutcome outcome);
    [[nodiscard]] quint64 nextRequestId();

    std::unique_ptr<OutputManagementPort> m_outputManagement;
    std::unique_ptr<JournalStore> m_journalStore;
    DisplayService::TransactionPortObserver *m_observer = nullptr;
    QTimer m_timeout;
    std::optional<Pending> m_pending;
    QTimer m_brightnessTimeout;
    std::optional<BrightnessPending> m_brightnessPending;
    quint64 m_machineLineage = 0;
    quint64 m_ownerGeneration = 0;
    quint64 m_nextRequestId = 1;
    // The latest accepted device fact frame. AGENT-NOTE: frames publish only
    // on change, and the outer runtime starts this port before the resident
    // service binds as observer (session-safety readiness gates that bind);
    // without the recorded frame the late bind would wait for the next
    // compositor change forever — the exact startup lost-wakeup that left
    // external brightness incapable in production.
    DisplayService::DeviceBrightnessFrame m_lastDevicesFrame;
    bool m_hasDevicesFrame = false;
    bool m_started = false;
    bool m_available = false;
};

} // namespace QindaQt::DisplayWriter
