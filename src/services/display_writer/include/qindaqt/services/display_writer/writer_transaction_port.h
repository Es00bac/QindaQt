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
    [[nodiscard]] virtual bool store(const DisplayTransaction::Journal &journal) = 0;
    [[nodiscard]] virtual bool clear() = 0;
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

    void setObserver(DisplayService::TransactionPortObserver *observer) override;
    void beginMachineLineage(quint64 machineLineage) override;
    [[nodiscard]] bool storeJournal(
        const DisplayTransaction::Journal &journal) override;
    [[nodiscard]] bool clearJournal() override;
    void requestApply(const DisplayTransaction::ApplyRequest &request) override;

private:
    struct Pending {
        quint64 machineLineage = 0;
        quint64 token = 0;
        quint64 requestId = 0;
        quint64 ownerGeneration = 0;
    };

    void outputManagementOwnerChanged(quint64 ownerGeneration,
                                      bool available) override;
    void outputManagementCompleted(quint64 ownerGeneration,
                                   quint64 requestId,
                                   CompletionOutcome outcome) override;
    void finishPending(DisplayTransaction::ApplyOutcome outcome);
    void finishDeferred(quint64 machineLineage, quint64 token,
                        DisplayTransaction::ApplyOutcome outcome);
    [[nodiscard]] quint64 nextRequestId();

    std::unique_ptr<OutputManagementPort> m_outputManagement;
    std::unique_ptr<JournalStore> m_journalStore;
    DisplayService::TransactionPortObserver *m_observer = nullptr;
    QTimer m_timeout;
    std::optional<Pending> m_pending;
    quint64 m_machineLineage = 0;
    quint64 m_ownerGeneration = 0;
    quint64 m_nextRequestId = 1;
    bool m_started = false;
    bool m_available = false;
};

} // namespace QindaQt::DisplayWriter
