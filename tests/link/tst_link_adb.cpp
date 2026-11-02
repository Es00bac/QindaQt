// SPDX-License-Identifier: GPL-3.0-or-later
// Tunnel manager against a fake adb. No real adb process is ever started.
#include <qindaqt/services/link_adb/tunnel_manager.h>

#include <QtTest/QtTest>

using namespace QindaQt::Link;

namespace {

class FakeAdb final : public AdbRunner {
public:
    void startTracking() override { ++trackStarts; }
    void stopTracking() override {}
    void run(const QStringList &args, std::function<void(int, const QByteArray &)> done) override
    {
        commands.append(args.join(QLatin1Char(' ')));
        if (args.contains(QLatin1String("pm"))) {
            const QString serial = args.at(1);
            const bool has = installed.contains(serial);
            done(has ? 0 : 1, has ? QByteArray("package:/data/app/x/base.apk\n") : QByteArray());
        } else {
            done(reverseExit, {});
        }
    }
    static QByteArray frame(const QString &text)
    {
        const QByteArray body = text.toLatin1();
        return QByteArray::number(body.size(), 16).rightJustified(4, '0') + body;
    }
    void emitDevices(const QString &text) { Q_EMIT trackData(frame(text)); }
    void endTracking() { Q_EMIT trackEnded(); }

    QStringList commands;
    QStringList installed;
    int reverseExit = 0;
    int trackStarts = 0;
};

TunnelOptions fast()
{
    TunnelOptions o;
    o.restartDelayMs = 20;
    o.recheckMs = 60;
    return o;
}

int reverseCount(const FakeAdb &a, const QString &serial)
{
    int n = 0;
    for (const QString &c : a.commands) {
        n += c.startsWith(QStringLiteral("-s %1 reverse tcp:47160 tcp:47160").arg(serial)) ? 1 : 0;
    }
    return n;
}

} // namespace

class LinkAdbTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void parserHandlesSplitAndMultipleUpdates()
    {
        TrackDevicesParser p;
        const QByteArray a = FakeAdb::frame(QStringLiteral("AAA\tdevice\nBBB\toffline\n"));
        QVERIFY(p.append(a.left(7)).isEmpty());
        const auto lists = p.append(a.mid(7) + FakeAdb::frame(QString()));
        QCOMPARE(lists.size(), 2);
        QCOMPARE(lists[0].size(), 2);
        QCOMPARE(lists[0][1].state, QStringLiteral("offline"));
        QVERIFY(lists[1].isEmpty());
    }

    void reversesOnlyDevicesWithTheApp()
    {
        FakeAdb adb;
        adb.installed = {QStringLiteral("AAA")};
        TunnelManager m(adb, fast());
        m.start();
        QCOMPARE(adb.trackStarts, 1);
        adb.emitDevices(QStringLiteral("AAA\tdevice\nBBB\tdevice\nCCC\tunauthorized\n"));
        QCOMPARE(reverseCount(adb, QStringLiteral("AAA")), 1);
        QCOMPARE(reverseCount(adb, QStringLiteral("BBB")), 0);
        for (const QString &c : adb.commands) {
            QVERIFY(!c.contains(QLatin1String("CCC")));
        }
        QCOMPARE(m.devices().size(), 2);
    }

    void reappliesAfterReconnectAndTrackerRestart()
    {
        FakeAdb adb;
        adb.installed = {QStringLiteral("AAA")};
        TunnelManager m(adb, fast());
        m.start();
        adb.emitDevices(QStringLiteral("AAA\tdevice\n"));
        adb.emitDevices(QString());
        adb.emitDevices(QStringLiteral("AAA\tdevice\n"));
        QCOMPARE(reverseCount(adb, QStringLiteral("AAA")), 2);
        adb.endTracking();
        QTRY_COMPARE_WITH_TIMEOUT(adb.trackStarts, 2, 1000);
        adb.emitDevices(QStringLiteral("AAA\tdevice\n"));
        QCOMPARE(reverseCount(adb, QStringLiteral("AAA")), 3);
    }

    void retriesWhenTheAppAppearsLater()
    {
        FakeAdb adb;
        TunnelManager m(adb, fast());
        m.start();
        adb.emitDevices(QStringLiteral("AAA\tdevice\n"));
        QCOMPARE(reverseCount(adb, QStringLiteral("AAA")), 0);
        adb.installed = {QStringLiteral("AAA")};
        QTRY_VERIFY_WITH_TIMEOUT(reverseCount(adb, QStringLiteral("AAA")) >= 1, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(m.devices().first().tunneled, 1000);
    }
};

QTEST_GUILESS_MAIN(LinkAdbTest)
#include "tst_link_adb.moc"
