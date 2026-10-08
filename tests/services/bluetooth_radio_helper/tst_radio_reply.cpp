// SPDX-License-Identifier: GPL-3.0-or-later
#include "../bluetooth_bluez_adapter/support/private_bus.h"
#include "../../../src/services/bluetooth_radio_helper/src/native_radio_wire_p.h"
#include "../../../src/services/bluetooth_radio_helper/src/native_radio_codec_p.h"
#include <qindaqt/services/bluetooth_radio_helper/qt_radio_power_port.h>
#include <qindaqt/services/bluetooth_radio_helper/radio_service_session.h>
#include <QtDBus/QDBusVirtualObject>
#include <QtDBus/QDBusMessage>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <memory>
#include <utility>

using namespace QindaQt::BluetoothRadio;
namespace {
class DeferredHelper final : public QDBusVirtualObject {
public:
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        if (message.member() != QLatin1String("ObserveAndUnblock")) return false;
        pending = message;
        issued = qdbus_cast<Request>(message.arguments().constFirst());
        received = true;
        return true;
    }
    QDBusMessage pending;
    Request issued;
    bool received = false;
};
class Fixture final {
public:
    Fixture() : helperBus(QStringLiteral("invalid-helper")), foreign(QStringLiteral("invalid-foreign")) {}
    ~Fixture() {
        port.reset();
        helperBus.unregisterObject(QString::fromLatin1(kPath));
        for (const auto &name : names) QDBusConnection::disconnectFromBus(name);
    }
    bool start() {
        if (!bus.start()) return false;
        session = std::make_unique<RadioServiceSession>(bus.address);
        if (!session->prepared()) return false;
        for (int i = 0; i < 2; ++i) names.append(QUuid::createUuid().toString(QUuid::Id128));
        helperBus = QDBusConnection::connectToBus(bus.address, names[0]);
        foreign = QDBusConnection::connectToBus(bus.address, names[1]);
        if (!session->authorityConnection().registerService(QStringLiteral("org.qindaqt.Bluetooth1"))
            || !helperBus.registerVirtualObject(QString::fromLatin1(kPath), &helper)
            || !helperBus.registerService(QString::fromLatin1(kService))) return false;
        port = std::make_unique<QtRadioPowerPort>(*session);
        return true;
    }
    quint64 begin(std::function<bool()> current = [] { return true; }) {
        return port->observeAndUnblock(QStringLiteral(":1.90"), QStringLiteral("/org/bluez/hci0"),
            QStringLiteral("12:34:56:78:9A:BC"), session->authorityConnection().baseService(), std::move(current));
    }
    QDBusMessage reply(const QString &nonce = {}) const {
        return helper.pending.createReply(QVariantList{QVariant::fromValue(Result{
            nonce.isEmpty() ? helper.issued.nonce : nonce,
            Disposition::VerifiedUnblocked, QStringLiteral("radio-unblocked")})});
    }
    QindaQt::Tests::PrivateBus bus;
    std::unique_ptr<RadioServiceSession> session;
    DeferredHelper helper;
    QDBusConnection helperBus, foreign;
    QStringList names;
    std::unique_ptr<QtRadioPowerPort> port;
};
Result result(const QSignalSpy &spy) { return qvariant_cast<Result>(spy.constFirst().at(1)); }
}
class RadioReplyTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void actualPendingReplyPreservesAndChecksSender_data() {
        QTest::addColumn<bool>("foreignSender");
        QTest::newRow("legitimate-peer") << false;
        QTest::newRow("foreign-correct-serial-and-nonce") << true;
    }
    void actualPendingReplyPreservesAndChecksSender() {
        QFETCH(bool, foreignSender);
        Fixture f; QVERIFY(f.start());
        NativeRadioWire caller; QVERIFY(caller.open(f.bus.address));
        const Request issued{QString(32, QLatin1Char('c')), QStringLiteral(":1.90"),
            QStringLiteral("/org/bluez/hci0"), QStringLiteral("12:34:56:78:9A:BC"),
            f.session->authorityConnection().baseService(), boottimeMilliseconds() + kRequestWindowMs,
            f.session->authorityConnection().baseService(), caller.uniqueOwner()};
        auto request = nativeMethod(f.helperBus.baseService(), kPath, kInterface, "ObserveAndUnblock");
        QVERIFY(appendNativeRequest(request.get(), issued));
        auto pending = caller.send(std::move(request), 1500); QVERIFY(pending);
        QTRY_VERIFY(f.helper.received);
        QCOMPARE(f.helper.pending.service(), caller.uniqueOwner());
        QCOMPARE(f.helper.issued, issued);
        auto &responder = foreignSender ? f.foreign : f.helperBus;
        QVERIFY(responder.send(f.reply()));
        QTRY_VERIFY_WITH_TIMEOUT(pending->completed(), 2000);
        auto reply = pending->take(); QVERIFY(reply);
        // A timeout/local error cannot satisfy this witness: inspect the real
        // returned frame before testing authority. The pending slot consumed
        // the held call serial, and the complete expected nonce/result remains.
        QCOMPARE(dbus_message_get_type(reply.get()), DBUS_MESSAGE_TYPE_METHOD_RETURN);
        const char *sender = dbus_message_get_sender(reply.get()); QVERIFY(sender);
        QCOMPARE(QString::fromUtf8(sender), responder.baseService());
        QVERIFY(dbus_message_get_reply_serial(reply.get()) != 0);
        Result decoded; QVERIFY(readNativeResult(reply.get(), &decoded));
        QCOMPARE(decoded.nonce, issued.nonce);
        QCOMPARE(decoded.disposition, Disposition::VerifiedUnblocked);
        QCOMPARE(pending->fromExpectedPeer(reply.get()), !foreignSender);
    }
    void exactNativeSenderCompletesIssuedDelegationOnce() {
        Fixture f; QVERIFY(f.start()); QSignalSpy done(f.port.get(), &RadioPowerPort::finished);
        QVERIFY(f.begin()); QTRY_VERIFY(f.helper.received);
        QCOMPARE(f.helper.issued.authorityOwner, f.session->authorityConnection().baseService());
        QVERIFY(f.helper.issued.transportCaller != f.helper.issued.authorityOwner);
        QCOMPARE(f.helper.pending.service(), f.helper.issued.transportCaller);
        QVERIFY(f.helperBus.send(f.reply()));
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(result(done).disposition, Disposition::VerifiedUnblocked);
        QVERIFY(f.helperBus.send(f.reply())); QTest::qWait(40); QCOMPARE(done.size(), 1);
    }
    void unsafeCompletion_data() {
        QTest::addColumn<QString>("fault");
        for (const auto *name : {"foreign-sender", "wrong-nonce", "malformed", "error",
             "helper-replaced", "authority-replaced", "revoked", "expired"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void unsafeCompletion() {
        QFETCH(QString, fault);
        Fixture f; QVERIFY(f.start()); QSignalSpy done(f.port.get(), &RadioPowerPort::finished);
        bool live = true;
        QVERIFY(f.begin([&live] { return live; })); QTRY_VERIFY(f.helper.received);
        if (fault == QLatin1String("foreign-sender")) {
            // Real captured call serial and correct nonce, sent by another private connection.
            QVERIFY(f.foreign.send(f.reply()));
        } else if (fault == QLatin1String("wrong-nonce")) {
            QVERIFY(f.helperBus.send(f.reply(QString(32, QLatin1Char('e')))));
        } else if (fault == QLatin1String("malformed")) {
            QVERIFY(f.helperBus.send(f.helper.pending.createReply(QVariantList{true})));
        } else if (fault == QLatin1String("error")) {
            QVERIFY(f.helperBus.send(f.helper.pending.createErrorReply(
                QStringLiteral("org.qindaqt.Test.Refused"), QStringLiteral("synthetic"))));
        } else {
            if (fault == QLatin1String("helper-replaced")) {
                QVERIFY(f.helperBus.unregisterService(QString::fromLatin1(kService)));
                QVERIFY(f.foreign.registerService(QString::fromLatin1(kService)));
            } else if (fault == QLatin1String("authority-replaced")) {
                QVERIFY(f.session->authorityConnection().unregisterService(QStringLiteral("org.qindaqt.Bluetooth1")));
                QVERIFY(f.foreign.registerService(QStringLiteral("org.qindaqt.Bluetooth1")));
            } else if (fault == QLatin1String("revoked")) live = false;
            if (fault != QLatin1String("expired")) QVERIFY(f.helperBus.send(f.reply()));
        }
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 3000);
        QCOMPARE(result(done).disposition, Disposition::Uncertain);
        QVERIFY(f.helperBus.send(f.reply()));
        QTest::qWait(40); QCOMPARE(done.size(), 1);
    }
    void cancellationInsideAdmissionCannotRevivePending() {
        Fixture f; QVERIFY(f.start()); QSignalSpy done(f.port.get(), &RadioPowerPort::finished);
        bool cancelNow = false, cancellationObserved = false; quint64 id = 0;
        id = f.begin([&] {
            if (cancelNow) { f.port->cancel(id); cancellationObserved = true; }
            return true;
        });
        QVERIFY(id); QTRY_VERIFY(f.helper.received); cancelNow = true;
        QVERIFY(f.helperBus.send(f.reply()));
        QTRY_VERIFY(cancellationObserved);
        QTest::qWait(100); QCOMPARE(done.size(), 0);
    }
    void ownerDestructionInsideAdmissionDoesNotUseRetiredPort() {
        Fixture f; QVERIFY(f.start());
        bool destroyNow = false;
        QVERIFY(f.begin([&] { if (destroyNow) f.port.reset(); return true; }));
        QTRY_VERIFY(f.helper.received); destroyNow = true;
        QVERIFY(f.helperBus.send(f.reply()));
        QTRY_VERIFY(!f.port);
    }
    void cancelledBeforeReplyStaysRetired() {
        Fixture f; QVERIFY(f.start()); QSignalSpy done(f.port.get(), &RadioPowerPort::finished);
        const auto id = f.begin(); QVERIFY(id); QTRY_VERIFY(f.helper.received);
        f.port->cancel(id); QVERIFY(f.helperBus.send(f.reply()));
        QTest::qWait(100); QCOMPARE(done.size(), 0);
    }
    void legacyQtOnlyPortIsDefinitivelyNoWrite() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        QtRadioPowerPort port(bus.connection); QSignalSpy done(&port, &RadioPowerPort::finished);
        QVERIFY(port.observeAndUnblock(QStringLiteral(":1.90"), QStringLiteral("/org/bluez/hci0"),
            QStringLiteral("12:34:56:78:9A:BC"), bus.connection.baseService(), [] { return true; }));
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(result(done).disposition, Disposition::NoWriteUnavailable);
    }
};
QTEST_GUILESS_MAIN(RadioReplyTest)
#include "tst_radio_reply.moc"
