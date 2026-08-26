// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtTypes>

namespace QindaQt::ShellVisibilityClient {

// Asynchronous transport seam. Implementations must bind invalidation signals
// and method calls to the exact unique owner passed to snapshot requests.
class CompositorVisibilityTransport : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~CompositorVisibilityTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;
    virtual void requestSnapshot(quint64 token, const QString &uniqueOwner) = 0;

Q_SIGNALS:
    void serviceOwnerChanged(const QString &uniqueOwner);
    void snapshotInvalidated(const QString &uniqueOwner);
    void snapshotReceived(quint64 token, const QString &uniqueOwner,
                          const QByteArray &payload);
    void snapshotFailed(quint64 token, const QString &uniqueOwner,
                        const QString &message);
};

} // namespace QindaQt::ShellVisibilityClient
