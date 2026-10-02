// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <functional>
namespace QindaQt::Session::DisplayPower {
// Same-thread supervisor boundary. The admitted ordinary peer identity and
// lineage predicate are borrowed, never replaced/reconnected after revocation.
// Method acceptance only admits a cause; actual mode comes from nonce receipts.
class ScopedDisplayPower final : public QObject {
    Q_OBJECT
public:
    ScopedDisplayPower(QDBusConnection bus, QString peerOwner, quint32 peerPid,
                      std::function<bool()> lineageLive, bool nativeExclusive,
                      QObject *parent = nullptr);
    ~ScopedDisplayPower() override;
    bool start();
    void stop(bool waitForAcknowledgement = false);
    bool available() const;
    bool off() const;
    bool acquire(const QString &id);
    void release(const QString &id);
Q_SIGNALS:
    void availabilityChanged(bool available);
    void powerChanged(bool off);
    void acquisitionFinished(const QString &id, bool admitted);
    void causeEnded(const QString &id, const QString &reason);
private Q_SLOTS:
    void inventoryReceipt(const QDBusMessage &message);
    void inventoryChanged(const QDBusMessage &message);
    void leaseEnded(const QDBusMessage &message);
private:
    bool live() const;
    void refresh();
    void setAvailable(bool available);
    void sendAcquire(const QString &id);
    QDBusConnection m_bus;
    QString m_owner;
    quint32 m_pid;
    std::function<bool()> m_lineage;
    QDBusServiceWatcher m_ownerWatch;
    QTimer m_deadline, m_renew;
    QSet<QString> m_causes;
    QString m_nonce;
    quint64 m_epoch = 0, m_serial = 0;
    bool m_native, m_started = false, m_available = false, m_off = false;
};
}
