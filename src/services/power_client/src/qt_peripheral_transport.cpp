// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/power_client/qt_peripheral_transport.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_types.h>
#include <qindaqt/services/power_protocol/peripheral_types.h>
#include <QtCore/QUuid>
#include <QtDBus/QDBusPendingCall>
namespace QindaQt::Power {
QtPeripheralTransport::QtPeripheralTransport(const QDBusConnection &connection,QObject *parent)
    : PeripheralTransport(parent),m_connection(connection) {}
QtPeripheralTransport::~QtPeripheralTransport() { bind({}); }
void QtPeripheralTransport::cancel() { m_token=0; m_nonce.clear(); }
void QtPeripheralTransport::bind(const QString &owner)
{
    cancel();
    if (!m_owner.isEmpty()) {
        m_connection.disconnect(m_owner,QString::fromLatin1(kObjectPath),QString::fromLatin1(kInterfaceName),
            QStringLiteral("PeripheralSnapshotReceipt"),this,SLOT(onReceipt(QDBusMessage)));
        m_connection.disconnect(m_owner,QString::fromLatin1(kObjectPath),QString::fromLatin1(kInterfaceName),
            QStringLiteral("PeripheralsChanged"),this,SLOT(onInvalidated(QDBusMessage)));
    }
    m_owner=owner; m_subscribed=false;
    if (owner.isEmpty() || !owner.startsWith(QLatin1Char(':'))) return;
    const bool receipt=m_connection.connect(owner,QString::fromLatin1(kObjectPath),QString::fromLatin1(kInterfaceName),
        QStringLiteral("PeripheralSnapshotReceipt"),this,SLOT(onReceipt(QDBusMessage)));
    const bool changed=m_connection.connect(owner,QString::fromLatin1(kObjectPath),QString::fromLatin1(kInterfaceName),
        QStringLiteral("PeripheralsChanged"),this,SLOT(onInvalidated(QDBusMessage)));
    m_subscribed=receipt && changed;
}
void QtPeripheralTransport::request(quint64 token)
{
    cancel();
    if (!m_subscribed || token==0) return;
    m_token=token; m_nonce=QUuid::createUuid().toString(QUuid::Id128).toLower();
    auto call=QDBusMessage::createMethodCall(m_owner,QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName),QStringLiteral("RequestPeripheralSnapshotWithReceipt"));
    call.setArguments({m_nonce});
    // Neither success nor error replies carry publication authority. The client
    // timeout handles old-service/failed dispatch; no Qt reply sender assumption.
    static_cast<void>(m_connection.asyncCall(call,1500));
}
bool QtPeripheralTransport::authentic(const QDBusMessage &message,const QString &member,const QString &signature) const
{
    return m_subscribed && !m_owner.isEmpty() && message.type()==QDBusMessage::SignalMessage
        && message.service()==m_owner && message.path()==QString::fromLatin1(kObjectPath)
        && message.interface()==QString::fromLatin1(kInterfaceName)
        && message.member()==member && message.signature()==signature;
}
void QtPeripheralTransport::onReceipt(const QDBusMessage &message)
{
    if (!m_token || !authentic(message,QStringLiteral("PeripheralSnapshotReceipt"),QStringLiteral("say"))) return;
    const auto args=message.arguments();
    if (args.size()!=2 || args[0].metaType()!=QMetaType::fromType<QString>()
        || args[1].metaType()!=QMetaType::fromType<QByteArray>() || args[0].toString()!=m_nonce) return;
    const auto payload=args[1].toByteArray();
    if (payload.size()>kMaxPeripheralPayloadBytes) return;
    const QString owner=m_owner; const quint64 token=m_token;
    cancel(); // consume before external publication; replay and reentrant stop cannot revive it.
    Q_EMIT receipt(owner,token,payload);
}
void QtPeripheralTransport::onInvalidated(const QDBusMessage &message)
{
    if (!authentic(message,QStringLiteral("PeripheralsChanged"),QStringLiteral("tt"))) return;
    const auto args=message.arguments();
    if (args.size()!=2 || args[0].metaType()!=QMetaType::fromType<quint64>()
        || args[1].metaType()!=QMetaType::fromType<quint64>()) return;
    Q_EMIT invalidated(m_owner,args[0].toULongLong(),args[1].toULongLong());
}
}
