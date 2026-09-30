// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/secret_portal/secret_broker.h>
#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QVariantMap>
namespace QindaQt::Services::SecretPortal {
// Attach before registering host with ExportAdaptors. Same-thread broker and
// bus outlive this adaptor. Only exact current frontend owner/UID can call it.
// Requests own bounded duplicated writable FDs/secure pages, cancel on close,
// timeout/owner/policy loss. Own FD copies close; transferred bytes/Qt marshalling
// copies cannot be recalled. No secret appears in a D-Bus backend result map.
class SecretPortalAdaptor final:public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.freedesktop.impl.portal.Secret")
    Q_PROPERTY(quint32 version READ version CONSTANT)
public:
    SecretPortalAdaptor(QObject &host,SecretBroker &,QDBusConnection,int timeoutMs=30000);
    ~SecretPortalAdaptor() override;
    quint32 version() const {return 1;}
public Q_SLOTS:
    quint32 RetrieveSecret(const QDBusObjectPath &,const QString &,const QDBusUnixFileDescriptor &,
                          const QVariantMap &,const QDBusMessage &call,QVariantMap &results);
private Q_SLOTS:
    void ownerChanged(const QString &,const QString &,const QString &);
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
