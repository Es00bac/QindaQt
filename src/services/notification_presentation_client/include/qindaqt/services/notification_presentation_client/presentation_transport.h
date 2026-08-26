// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtTypes>

namespace QindaQt::Services::NotificationPresentationClient {

// Asynchronous transport seam. Implementations bind every signal and request
// to the exact unique owner supplied by the client and never log accessToken.
class PresentationTransport : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~PresentationTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;
    virtual void registerPresenter(quint64 token, const QString &uniqueOwner,
                                   const QString &accessToken) = 0;
    virtual void requestSnapshot(quint64 token, const QString &uniqueOwner) = 0;
    virtual void releasePresenter(const QString &uniqueOwner) = 0;
    virtual void dismiss(quint64 token, const QString &uniqueOwner, quint32 id) = 0;
    virtual void invokeAction(quint64 token, const QString &uniqueOwner, quint32 id,
                              const QString &actionKey,
                              const QString &activationToken) = 0;

Q_SIGNALS:
    void serviceOwnerChanged(const QString &uniqueOwner);
    void snapshotInvalidated(const QString &uniqueOwner, const QString &epoch,
                             quint64 revision);
    void snapshotReceived(quint64 token, const QString &uniqueOwner,
                          const QVariantMap &wire);
    void requestFailed(quint64 token, const QString &uniqueOwner,
                       const QString &errorName, const QString &message);
    void operationFinished(quint64 token, const QString &uniqueOwner,
                           const QVariantMap &result);
    void operationFailed(quint64 token, const QString &uniqueOwner,
                         const QString &errorName, const QString &message);
};

} // namespace QindaQt::Services::NotificationPresentationClient
