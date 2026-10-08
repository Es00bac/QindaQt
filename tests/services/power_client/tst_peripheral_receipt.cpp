// SPDX-License-Identifier: GPL-3.0-or-later
#include "../power_service/support/private_bus.h"
#include "support/fake_power_transport.h"
#include <qindaqt/services/power_client/qt_peripheral_transport.h>
#include <qindaqt/services/power_client/peripheral_client.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <QtDBus/QDBusContext>
#include <QtCore/QPointer>
#include <QtTest>
using namespace QindaQt::Power;
using namespace QindaQt::Tests;
class ReceiptService : public QObject, protected QDBusContext {
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface","org.qindaqt.Power1")
public:
 QString nonce,destination;
public Q_SLOTS:
 bool RequestPeripheralSnapshotWithReceipt(const QString &value) {
    nonce=value; destination=message().service(); return true;
 }
};
class PeripheralReceiptTest : public QObject {
 Q_OBJECT
private:
 static QByteArray payload(quint64 epoch=11,quint64 revision=1) {
    PeripheralSnapshot value;value.epoch=epoch;value.revision=revision;value.availability=Availability::Ready;
    PeripheralBattery row;row.handle={epoch,QStringLiteral("speaker")};row.model=QStringLiteral("Speaker");
    value.devices={row};QByteArray bytes;const bool encoded=encodePeripheralSnapshot(value,bytes);Q_ASSERT(encoded);return bytes;
 }
 static bool send(const QDBusConnection &connection,const QString &destination,const QString &nonce,const QByteArray &bytes) {
    auto signal=QDBusMessage::createTargetedSignal(destination,QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName),QStringLiteral("PeripheralSnapshotReceipt"));
    signal.setArguments({nonce,bytes});return connection.send(signal);
 }
private Q_SLOTS:
 void genuineForeignReplayAndCancellation() {
    PrivateBus bus;QVERIFY(bus.start());auto owner=bus.openConnection(QStringLiteral("owner"));
    auto foreign=bus.openConnection(QStringLiteral("foreign"));ReceiptService service;
    QVERIFY(owner.registerObject(QString::fromLatin1(kObjectPath),&service,QDBusConnection::ExportAllSlots));
    QtPeripheralTransport transport(bus.connection);QSignalSpy received(&transport,&PeripheralTransport::receipt);
    transport.bind(owner.baseService());transport.request(1);
    QTRY_VERIFY(!service.nonce.isEmpty());const QString first=service.nonce;
    QVERIFY(send(foreign,service.destination,first,payload()));QTest::qWait(30);QCOMPARE(received.size(),0);
    QVERIFY(send(owner,service.destination,QString(32,QLatin1Char('0')),payload()));QTest::qWait(30);QCOMPARE(received.size(),0);
    QVERIFY(send(owner,service.destination,first,payload()));QTRY_COMPARE(received.size(),1);
    QVERIFY(send(owner,service.destination,first,payload()));QTest::qWait(30);QCOMPARE(received.size(),1);
    service.nonce.clear();transport.request(2);QTRY_VERIFY(!service.nonce.isEmpty());
    const QString canceled=service.nonce;transport.cancel();
    QVERIFY(send(owner,service.destination,canceled,payload()));QTest::qWait(30);QCOMPARE(received.size(),1);
    transport.bind(foreign.baseService());QVERIFY(send(owner,service.destination,canceled,payload()));
    QTest::qWait(30);QCOMPARE(received.size(),1);
 }
 void ownerReplacementEpochAndDeleteOnPublication() {
    PrivateBus bus;QVERIFY(bus.start());auto owner=bus.openConnection(QStringLiteral("owner"));ReceiptService service;
    QVERIFY(owner.registerObject(QString::fromLatin1(kObjectPath),&service,QDBusConnection::ExportAllSlots));
    FakePowerTransport mainTransport;PowerClient authority(&mainTransport);
    QtPeripheralTransport transport(bus.connection);
    auto *client=new PeripheralClient(&authority,&transport);QPointer<PeripheralClient> guard(client);
    authority.start();mainTransport.announceOwner(owner.baseService());
    QVERIFY(!mainTransport.fetches.isEmpty());mainTransport.reply(mainTransport.fetches.last(),powerClientSnapshot());
    client->start();QTRY_VERIFY(!service.nonce.isEmpty());const QString oldNonce=service.nonce;
    mainTransport.announceOwner(QString{});QVERIFY(!client->hasSnapshot());
    QVERIFY(send(owner,service.destination,oldNonce,payload()));QTest::qWait(30);QVERIFY(!client->hasSnapshot());
    service.nonce.clear();mainTransport.announceOwner(owner.baseService());
    mainTransport.reply(mainTransport.fetches.last(),powerClientSnapshot(12));
    QTRY_VERIFY(!service.nonce.isEmpty());
    QVERIFY(send(owner,service.destination,service.nonce,payload(11)));QTest::qWait(30);QVERIFY(!client->hasSnapshot());
    service.nonce.clear();client->refresh();QTRY_VERIFY(!service.nonce.isEmpty());
    connect(client,&PeripheralClient::changed,client,[client] { if(client->hasSnapshot()) delete client; });
    QVERIFY(send(owner,service.destination,service.nonce,payload(12)));QTRY_VERIFY(guard.isNull());
 }
 void equalRevisionContradictionAndStopOnPublication() {
    PrivateBus bus;QVERIFY(bus.start());auto owner=bus.openConnection(QStringLiteral("owner"));ReceiptService service;
    QVERIFY(owner.registerObject(QString::fromLatin1(kObjectPath),&service,QDBusConnection::ExportAllSlots));
    FakePowerTransport mainTransport;PowerClient authority(&mainTransport);
    QtPeripheralTransport transport(bus.connection);PeripheralClient client(&authority,&transport);
    authority.start();mainTransport.announceOwner(owner.baseService());
    mainTransport.reply(mainTransport.fetches.last(),powerClientSnapshot());client.start();
    QTRY_VERIFY(!service.nonce.isEmpty());QVERIFY(send(owner,service.destination,service.nonce,payload()));
    QTRY_VERIFY(client.hasSnapshot());
    service.nonce.clear();client.refresh();QTRY_VERIFY(!service.nonce.isEmpty());
    PeripheralSnapshot changed;QVERIFY(decodePeripheralSnapshot(payload(),changed));
    changed.devices[0].model=QStringLiteral("Contradiction");QByteArray bytes;QVERIFY(encodePeripheralSnapshot(changed,bytes));
    QVERIFY(send(owner,service.destination,service.nonce,bytes));QTRY_VERIFY(!client.hasSnapshot());
    service.nonce.clear();client.refresh();QTRY_VERIFY(!service.nonce.isEmpty());
    connect(&client,&PeripheralClient::changed,&client,[&client] {if(client.hasSnapshot())client.stop();});
    const auto nonce=service.nonce;QVERIFY(send(owner,service.destination,nonce,payload(11,2)));
    QTRY_COMPARE(client.reasonCode(),QStringLiteral("stopped"));QVERIFY(!client.hasSnapshot());
    QVERIFY(send(owner,service.destination,nonce,payload(11,2)));QTest::qWait(30);QVERIFY(!client.hasSnapshot());
 }
 void deleteTransportOnReceipt() {
    PrivateBus bus;QVERIFY(bus.start());auto owner=bus.openConnection(QStringLiteral("owner"));ReceiptService service;
    QVERIFY(owner.registerObject(QString::fromLatin1(kObjectPath),&service,QDBusConnection::ExportAllSlots));
    auto *transport=new QtPeripheralTransport(bus.connection);QPointer<QtPeripheralTransport> guard(transport);
    connect(transport,&PeripheralTransport::receipt,transport,[transport] {delete transport;});
    transport->bind(owner.baseService());transport->request(1);QTRY_VERIFY(!service.nonce.isEmpty());
    QVERIFY(send(owner,service.destination,service.nonce,payload()));QTRY_VERIFY(guard.isNull());
 }
};
QTEST_GUILESS_MAIN(PeripheralReceiptTest)
#include "tst_peripheral_receipt.moc"
