// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace QindaQt::Services::SettingsClient {

// Thread-confined asynchronous transport seam. Implementations must subscribe
// to SettingsChanged from an exact unique owner before emitting ownerChanged;
// the client immediately requests its baseline from that same owner.
class SettingsTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~SettingsTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;
    virtual void requestSnapshot(quint64 token, const QString &owner,
                                 const QStringList &keys) = 0;
    virtual void commit(quint64 token, const QString &owner, const QString &epoch,
                        quint64 baseRevision, const QVariantList &operations) = 0;
    virtual void requestActivation() = 0;

Q_SIGNALS:
    void ownerChanged(const QString &uniqueOwner);
    void settingsChanged(const QString &uniqueOwner, const QString &epoch,
                         quint64 revision, const QStringList &keys);
    void snapshotReceived(quint64 token, const QString &uniqueOwner,
                          const QVariantMap &wire);
    void commitReceived(quint64 token, const QString &uniqueOwner,
                        const QVariantMap &wire);
    void requestFailed(quint64 token, const QString &uniqueOwner,
                       const QString &errorName, const QString &message);
    // Exactly one terminal activation signal follows each accepted attempt.
    // Completion does not imply an owner survived long enough to bind.
    void activationCompleted();
    void activationFailed(const QString &message);
    void busDisconnected();
};

} // namespace QindaQt::Services::SettingsClient
