// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

namespace QindaQt::Network::Client {

// Thread-confined asynchronous transport seam for Network N0. N0 ships no
// implementation: tests inject a fake backend and the later N1 slice provides
// the platform connection-management resident adapter behind this exact
// interface. Implementations must bind every received/failed signal to the
// exact unique owner captured at call time, and must emit ownerChanged before
// delivering any payload from a different owner.
class NetworkTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~NetworkTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;
    // Requests the canonical snapshot payload for the bound owner.
    virtual void requestSnapshot(quint64 token, const QString &owner) = 0;
    // Serialized mutation; `parameters` is bounded, secret-free, and was
    // admitted by the model's intent policy before this call.
    virtual void requestOperation(quint64 token, const QString &owner,
                                  quint64 epoch, quint64 revision,
                                  OperationKind kind,
                                  const QVariantMap &parameters) = 0;

Q_SIGNALS:
    void ownerChanged(const QString &uniqueOwner);
    // A change hint is an invalidation only: the client must refetch the
    // canonical snapshot and never reconstruct truth from this signal.
    void snapshotInvalidated(const QString &uniqueOwner);
    void snapshotReceived(quint64 token, const QString &uniqueOwner,
                          const QByteArray &payload);
    void operationReceived(quint64 token, const QString &uniqueOwner,
                           const QByteArray &payload);
    void requestFailed(quint64 token, const QString &uniqueOwner,
                       const QString &errorName, const QString &message);
    void busDisconnected();
};

} // namespace QindaQt::Network::Client
