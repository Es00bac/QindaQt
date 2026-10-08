// SPDX-License-Identifier: GPL-3.0-or-later
#include "../bluetooth_bluez_adapter/support/private_bus.h"
#include "../../../src/services/bluetooth_radio_helper/src/native_radio_wire_p.h"
#include <qindaqt/services/bluetooth_radio_helper/radio_service_session.h>
#include <QtTest/QTest>
#include <QtCore/QTemporaryDir>

using namespace QindaQt::BluetoothRadio;
class RadioSessionTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void capturesOneBusWithDistinctOwnedConnections() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        RadioServiceSession session(bus.address);
        QVERIFY(session.prepared()); QVERIFY(!session.legacyStartupAllowed());
        auto qt = session.authorityConnection(); QVERIFY(qt.isConnected());
        QVERIFY(qt.registerService(QStringLiteral("org.qindaqt.Test.RadioSession")));
        NativeRadioWire observer; QVERIFY(observer.open(bus.address, false));
        QCOMPARE(observer.owner(QStringLiteral("org.qindaqt.Test.RadioSession")), qt.baseService());
        QVERIFY(observer.uniqueOwner() != qt.baseService());
    }
    void explicitWrongGuidCannotFallBack() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        const auto address = bus.address.section(QStringLiteral(",guid="), 0, 0)
            + QStringLiteral(",guid=00000000000000000000000000000000");
        RadioServiceSession session(address);
        QVERIFY(!session.prepared()); QVERIFY(!session.legacyStartupAllowed());
    }
    void samePathDifferentBrokerRejectsCapturedGuid() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        struct Broker {
            QProcess process;
            ~Broker() {
                process.terminate();
                if (!process.waitForFinished(1000)) { process.kill(); process.waitForFinished(); }
            }
            bool start(const QString &address) {
                process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                    {QStringLiteral("--session"), QStringLiteral("--nofork"),
                     QStringLiteral("--nopidfile"), QStringLiteral("--print-address=1"),
                     QStringLiteral("--address=") + address});
                return process.waitForStarted() && process.waitForReadyRead();
            }
        };
        const auto address = QStringLiteral("unix:path=") + directory.path() + QStringLiteral("/bus");
        QString captured;
        {
            Broker first; QVERIFY(first.start(address));
            NativeRadioWire original; QVERIFY(original.open(address, false));
            captured = original.pinnedAddress();
        }
        Broker replacement; QVERIFY(replacement.start(address));
        RadioServiceSession session(captured);
        QVERIFY(!session.prepared()); QVERIFY(!session.legacyStartupAllowed());
    }
    void selectedConnectionLossCannotEnableFallback() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        RadioServiceSession session(bus.address); QVERIFY(session.prepared());
        bus.process.terminate(); QVERIFY(bus.process.waitForFinished(1000));
        QTRY_VERIFY(!session.prepared()); QVERIFY(!session.legacyStartupAllowed());
    }
    void malformedAddress_data() {
        QTest::addColumn<QString>("address");
        QTest::newRow("empty") << QString{};
        QTest::newRow("tcp") << QStringLiteral("tcp:host=localhost,port=1");
        QTest::newRow("two-endpoints") << QStringLiteral("unix:path=/a,abstract=b");
        QTest::newRow("bad-guid") << QStringLiteral("unix:path=/a,guid=bad");
        QTest::newRow("duplicate-guid") << QStringLiteral("unix:path=/a,guid=00000000000000000000000000000000,guid=00000000000000000000000000000000");
        QTest::newRow("two-addresses") << QStringLiteral("unix:path=/a;unix:path=/b");
    }
    void malformedAddress() {
        QFETCH(QString, address);
        QVERIFY(!NativeRadioWire::boundedAddress(address));
        RadioServiceSession session(address); QVERIFY(!session.prepared());
        QCOMPARE(session.legacyStartupAllowed(), address.isEmpty());
    }
    void absentBeforeSelectionAllowsOnlyLegacyStartup() {
        RadioServiceSession session(QStringLiteral("unix:path=/nonexistent-qinda-radio-session-fixture"));
        QVERIFY(!session.prepared()); QVERIFY(session.legacyStartupAllowed());
    }
};
QTEST_GUILESS_MAIN(RadioSessionTest)
#include "tst_radio_session.moc"
