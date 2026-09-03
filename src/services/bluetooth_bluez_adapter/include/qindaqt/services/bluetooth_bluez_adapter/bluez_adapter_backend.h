// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/adapter_backend.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Bluetooth
{

// Production AdapterBackend over the BlueZ D-Bus API. The connection is
// injected: the resident service passes the system bus, tests pass a private
// bus carrying a fake `org.bluez`, and nothing in this class ever contacts
// the host system bus, rfkill, or radios on its own.
//
// AGENT-CONTRACT: This class implements the AdapterBackend port contract from
// bluetooth_model exactly: start() returns the run generation before that run
// publishes, every publication and operation completion is fenced by
// generation, the backend owns the caller-scoped reference-counted discovery
// lease table, and pairing/trust authority stays in BlueZ (no Pair, Trust,
// Untrust, RemoveDevice, or agent calls; Paired/Trusted are read-only
// observations). The transport boundary and its exact-owner fencing are
// recorded in ADR-0056 and the Bluetooth service architecture page.
class BluezAdapterBackend final : public AdapterBackend
{
public:
    explicit BluezAdapterBackend(const QDBusConnection &connection,
                                 QObject *parent = nullptr);
    ~BluezAdapterBackend() override;

    BluezAdapterBackend(const BluezAdapterBackend &) = delete;
    BluezAdapterBackend &operator=(const BluezAdapterBackend &) = delete;

    [[nodiscard]] quint64 start() override;
    void stop() override;
    void submit(quint64 operationId, const BackendRequest &request) override;
    void releaseOwner(const QString &callerId) override;

private:
    struct State;
    std::unique_ptr<State> d;

    void applySubmit(quint64 operationId, const BackendRequest &request);
    void submitSetPower(quint64 operationId, const BackendRequest &request);
    void submitAcquire(quint64 operationId, const BackendRequest &request);
    void submitRelease(quint64 operationId, const BackendRequest &request);
    void submitConnect(quint64 operationId, const BackendRequest &request);
    void submitDisconnect(quint64 operationId, const BackendRequest &request);
    void insertDeviceCall(quint64 callId, quint64 operationId,
                          const BackendRequest &request, OperationKind kind);
    void handleCallFinished(quint64 callId, bool succeeded, const QString &errorName);
    void handleOwnerReplaced();
    void applyProperties(const QString &path, const QString &interfaceName,
                         const QVariantMap &changed, const QStringList &invalidated);
    void retireObjects(const QString &path, const QStringList &interfaces);
    void publish();
    void finishOperation(quint64 operationId, BackendOperationStatus status,
                         const QString &reasonCode);
};

} // namespace QindaQt::Bluetooth
