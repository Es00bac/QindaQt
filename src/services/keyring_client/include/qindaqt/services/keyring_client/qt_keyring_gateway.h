// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "keyring_gateway.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusObjectPath>
#include <memory>
namespace QindaQt::Services::KeyringClient {
// Borrows a same-thread bus connection, never disconnects another consumer.
// Pins org.freedesktop.secrets' unique owner and same-UID credentials; this is
// ordinary user-service authority, NOT the privileged PAM activation proof.
// Uses public Secret Service plain sessions; framework plaintext allocations
// are short-lived, owned wire copies wiped, not universally locked pages.
// Construction disables process cores/dumpability before any secret request;
// failure leaves unavailable. No activation, password capture or storage access.
class QtKeyringGateway final : public KeyringGateway {
    Q_OBJECT
public:
    explicit QtKeyringGateway(QDBusConnection bus, QObject *parent = nullptr);
    ~QtKeyringGateway() override;
    bool available() const override;
    void request(quint64, Request, const QString & = {}, const QString & = {}) override;
    void cancel() override;
private Q_SLOTS:
    void promptCompleted(bool, const QDBusVariant &, const QDBusMessage &);
    void disconnected();
    void collectionStateChanged(const QDBusObjectPath &,bool,bool,const QDBusMessage &);
    void policyStateReceipt(const QString &,const QVariantMap &,const QDBusMessage &);
    void policyStateChanged(const QVariantMap &,const QDBusMessage &);
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
