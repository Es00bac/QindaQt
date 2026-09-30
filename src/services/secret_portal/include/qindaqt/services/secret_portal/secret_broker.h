// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/secure_buffer.h>
#include <QObject>
#include <QDBusConnection>
#include <memory>
namespace QindaQt::Services::SecretPortal {
using SecretPages=std::shared_ptr<qindaqt::keyring::SecureBuffer>;
enum class BrokerError {None,Cancelled,Unavailable,Failed};
// Same-thread async acquisition. Borrowed broker outlives its adaptor. Nonzero
// tokens fence results; cancel suppresses completion and clears owned pages.
// completed transfers shared secure ownership, never strings/master store keys.
class SecretBroker:public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void retrieve(quint64 token,const QString &appId)=0;
    virtual void cancel(quint64 token)=0;
    virtual bool admitted() const=0;
Q_SIGNALS:
    void completed(quint64 token,SecretPages secret,BrokerError error);
    void authorityLost();
};
// Pins native and Secret Service names to one same-UID unique owner. Exact
// owner/policy loss retires prompts/results; no activation/replay/reconnection.
class QtKeyringPortalBroker final:public SecretBroker {
    Q_OBJECT
public:
    explicit QtKeyringPortalBroker(QDBusConnection,QObject *parent=nullptr);
    ~QtKeyringPortalBroker() override;
    void retrieve(quint64,const QString &) override;
    void cancel(quint64) override;
    bool admitted() const override;
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
