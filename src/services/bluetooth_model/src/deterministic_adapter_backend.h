// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/adapter_backend.h>

#include <QtCore/QHash>

namespace QindaQt::Bluetooth
{

// AGENT-NOTE: This header is private to the bluetooth_model build and test
// directories. It exists so qualification can populate platform state that
// production code never fabricates. The public factory in
// deterministic_backend_factory.h hands out only the neutral AdapterBackend
// view whose inventory starts empty.
class DeterministicAdapterBackend final : public AdapterBackend
{
public:
    explicit DeterministicAdapterBackend(QObject *parent = nullptr);

    [[nodiscard]] quint64 start() override;
    void stop() override;
    void submit(quint64 operationId, const BackendRequest &request) override;
    void releaseOwner(const QString &callerId) override;

    // Replaces the simulated platform state and republishes it to the model
    // when a run is active. Values are used verbatim; the model's fail-closed
    // validation is the authority that rejects malformed state.
    void setInventory(const BackendInventory &inventory);

    [[nodiscard]] quint64 generation() const noexcept { return m_generation; }
    [[nodiscard]] bool isRunning() const noexcept { return m_running; }
    [[nodiscard]] int submitCalls() const noexcept { return m_submitCalls; }
    [[nodiscard]] int releaseOwnerCalls() const noexcept { return m_releaseOwnerCalls; }
    [[nodiscard]] const BackendInventory &inventory() const noexcept { return m_state; }

private:
    struct LeaseKey {
        QString callerId;
        QString adapterAddress;

        friend bool operator==(const LeaseKey &, const LeaseKey &) = default;
    };

    friend size_t qHash(const LeaseKey &key, size_t seed) noexcept
    {
        return qHashMulti(seed, key.callerId, key.adapterAddress);
    }

    // Runs on a queued invocation from submit(); never synchronously.
    void applySubmit(quint64 operationId, const BackendRequest &request);
    void publish();
    void finish(const quint64 operationId, const BackendOperationStatus status,
                const QString &reasonCode);
    void recomputeDiscovery();
    [[nodiscard]] quint32 leaseTotal(const QString &adapterAddress) const;

    BackendInventory m_state;
    QHash<LeaseKey, quint32> m_leases;
    quint64 m_generation = 0;
    int m_submitCalls = 0;
    int m_releaseOwnerCalls = 0;
    bool m_running = false;
};

} // namespace QindaQt::Bluetooth
