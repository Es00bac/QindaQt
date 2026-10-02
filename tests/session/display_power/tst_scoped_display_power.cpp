// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../services/power_service/support/private_bus.h"
#include <qindaqt/session/display_power/scoped_display_power.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusContext>
#include <QDBusPendingReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <unistd.h>
#include <utility>
using namespace QindaQt;
using namespace QindaQt::Session::DisplayPower;
namespace {
const QString Path = QStringLiteral("/org/qindaqt/KWin/DisplayPower");
const QString Interface = QStringLiteral("org.qindaqt.KWin.DisplayPower1");
QByteArray validInventory() {
    return R"([{"id":"output-a","name":"Model output","dpms":true,"on":true,"geometry":[0,0,640,480]}])";
}
class Peer final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.DisplayPower1")
public:
    explicit Peer(QDBusConnection connection) : bus(std::move(connection)) {}
    void receipt(const QString &nonce) {
        auto signal = QDBusMessage::createTargetedSignal(actor, Path, Interface, QStringLiteral("InventoryReceipt"));
        signal << nonce << qulonglong{9} << true << admitted << inventory;
        QVERIFY(bus.send(signal));
    }
    void completeDelayed() {
        for (const auto &pending : delayed) QVERIFY(bus.send(pending.createReply(QVariantList{true})));
        delayed.clear();
    }
public Q_SLOTS:
    void RequestInventoryWithReceipt(const QString &nonce) {
        ++queries; lastNonce = nonce;
        if (replyInventory) receipt(nonce);
    }
    bool SetNativeAdmission(qulonglong epoch, bool enabled) {
        if (message().service() != actor || epoch != 9) return false;
        admitted = enabled; ++admissions;
        if (!enabled) causes.clear();
        return true;
    }
    bool AcquireBlank(qulonglong epoch, const QString &id, qulonglong deadline) {
        if (!admitted || epoch != 9 || message().service() != actor || deadline == 0) return false;
        causes.insert(id);
        if (delayAcquire) { setDelayedReply(true); delayed.push_back(message()); }
        return true;
    }
    bool ReleaseBlank(qulonglong epoch, const QString &id) {
        if (epoch != 9 || message().service() != actor) return false;
        return causes.remove(id) != 0;
    }
    bool Ping() { return true; }
public:
    QDBusConnection bus;
    QString actor, lastNonce;
    QByteArray inventory = validInventory();
    QSet<QString> causes;
    QList<QDBusMessage> delayed;
    int queries = 0, admissions = 0;
    bool admitted = false, replyInventory = true, delayAcquire = false;
};
class Fixture final {
public:
    bool start() {
        if (!bus.start()) return false;
        server = bus.openConnection(QStringLiteral("display-peer"));
        actor = bus.openConnection(QStringLiteral("supervisor"));
        stranger = bus.openConnection(QStringLiteral("replacement"));
        peer = std::make_unique<Peer>(server); peer->actor = actor.baseService();
        return server.registerService(QString(CompositorNames::service))
            && server.registerObject(Path, peer.get(), QDBusConnection::ExportAllSlots)
            && actor.registerService(QStringLiteral("org.qindaqt.Session1"));
    }
    std::unique_ptr<ScopedDisplayPower> client(bool enabled = true) {
        return std::make_unique<ScopedDisplayPower>(actor, server.baseService(), quint32(::getpid()), [&] { return lineage; }, enabled);
    }
    QDBusPendingReply<bool> barrier() {
        return actor.asyncCall(QDBusMessage::createMethodCall(server.baseService(), Path, Interface, QStringLiteral("Ping")));
    }
    Tests::PrivateBus bus;
    QDBusConnection server{QStringLiteral("invalid")}, actor{QStringLiteral("invalid")}, stranger{QStringLiteral("invalid")};
    std::unique_ptr<Peer> peer;
    bool lineage = true;
};
}
class ScopedDisplayPowerTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void defaultOffAndMissingLineageDoNotAdmit() {
        Fixture f; QVERIFY(f.start());
        auto disabled = f.client(false); QVERIFY(!disabled->start());
        f.lineage = false; auto absent = f.client(); QVERIFY(!absent->start());
        QCOMPARE(f.peer->queries, 0); QCOMPARE(f.peer->admissions, 0);
    }
    void actualPeerReceiptAdmitsAndReleaseCancelsOnlyOurCause() {
        Fixture f; QVERIFY(f.start()); auto client = f.client(); QVERIFY(client->start()); QTRY_VERIFY(client->available());
        const QString id(32, QLatin1Char('a')); QVERIFY(client->acquire(id)); QTRY_VERIFY(f.peer->causes.contains(id));
        client->release(id); QTRY_VERIFY(f.peer->causes.isEmpty());
        client->stop(); QTRY_VERIFY(!f.peer->admitted);
    }
    void cancelBeforeLateAdmissionReplyDoesNotResurrect() {
        Fixture f; QVERIFY(f.start()); f.peer->delayAcquire = true;
        auto client = f.client(); QVERIFY(client->start()); QTRY_VERIFY(client->available());
        QSignalSpy completion(client.get(), &ScopedDisplayPower::acquisitionFinished);
        const QString id(32, QLatin1Char('b')); QVERIFY(client->acquire(id)); QTRY_COMPARE(f.peer->delayed.size(), 1);
        client->release(id); QTRY_VERIFY(f.peer->causes.isEmpty());
        f.peer->completeDelayed(); auto barrier = f.barrier(); QTRY_VERIFY(barrier.isFinished());
        QVERIFY(!barrier.isError()); QCOMPARE(completion.size(), 0);
    }
    void incompleteInventory_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray("[]");
        QTest::newRow("unsupported") << QByteArray(validInventory()).replace("true,\"on\"", "false,\"on\"");
        QTest::newRow("zero-geometry") << QByteArray(validInventory()).replace("640,480", "0,480");
        QTest::newRow("duplicate") << (QByteArray("[") + validInventory().mid(1,validInventory().size()-2) + ',' + validInventory().mid(1,validInventory().size()-2) + ']');
    }
    void incompleteInventory() {
        QFETCH(QByteArray, bytes); Fixture f; QVERIFY(f.start()); f.peer->inventory = bytes;
        auto client = f.client(); QVERIFY(client->start()); QTRY_COMPARE(f.peer->queries, 1);
        auto barrier = f.barrier(); QTRY_VERIFY(barrier.isFinished()); QVERIFY(!barrier.isError());
        QVERIFY(!client->available()); QCOMPARE(f.peer->admissions, 0);
    }
    void spoofedAndUncorrelatedReceiptsCannotAdmit() {
        Fixture f; QVERIFY(f.start()); f.peer->replyInventory = false;
        auto client = f.client(); QVERIFY(client->start()); QTRY_COMPARE(f.peer->queries, 1);
        f.peer->receipt(QString(32, QLatin1Char('c')));
        auto spoof = QDBusMessage::createTargetedSignal(f.actor.baseService(), Path, Interface, QStringLiteral("InventoryReceipt"));
        spoof << f.peer->lastNonce << qulonglong{9} << true << false << validInventory(); QVERIFY(f.stranger.send(spoof));
        auto barrier = f.barrier(); QTRY_VERIFY(barrier.isFinished()); QVERIFY(!client->available()); QCOMPARE(f.peer->admissions, 0);
        f.peer->receipt(f.peer->lastNonce); QTRY_VERIFY(client->available());
    }
    void replacementNeverReceivesOldCleanupOrAutomaticReconnect() {
        Fixture f; QVERIFY(f.start()); auto client = f.client(); QVERIFY(client->start()); QTRY_VERIFY(client->available());
        const QString id(32, QLatin1Char('d')); QVERIFY(client->acquire(id)); QTRY_VERIFY(f.peer->causes.contains(id));
        QVERIFY(f.server.unregisterService(QString(CompositorNames::service)));
        Peer replacement(f.stranger); replacement.actor = f.actor.baseService();
        QVERIFY(f.stranger.registerObject(Path, &replacement, QDBusConnection::ExportAllSlots));
        QVERIFY(f.stranger.registerService(QString(CompositorNames::service)));
        QTRY_VERIFY(!client->available()); QTRY_VERIFY(!f.peer->admitted);
        QVERIFY(!client->acquire(QString(32, QLatin1Char('e')))); QCOMPARE(replacement.queries, 0); QCOMPARE(replacement.admissions, 0);
        f.stranger.unregisterObject(Path);
    }
};
QTEST_GUILESS_MAIN(ScopedDisplayPowerTests)
#include "tst_scoped_display_power.moc"
