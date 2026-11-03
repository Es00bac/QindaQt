// SPDX-License-Identifier: GPL-3.0-or-later
// PhoneSetup against a fake adb. No real adb process is ever started and no phone is touched.
#include <qindaqt/services/link_service/phone_setup.h>

#include <QtCore/QJsonArray>
#include <QtTest/QtTest>

using namespace QindaQt::Link;

namespace {

class FakeAdb final : public AdbRunner {
public:
    void startTracking() override {}
    void stopTracking() override {}
    void run(const QStringList &args, std::function<void(int, const QByteArray &)> done) override
    {
        commands.append(args.join(QLatin1Char(' ')));
        if (hold) {
            held.append(done);
            return;
        }
        const QString joined = commands.last();
        for (auto it = failing.cbegin(); it != failing.cend(); ++it) {
            if (joined.contains(*it)) {
                done(1, {});
                return;
            }
        }
        for (auto it = outputs.cbegin(); it != outputs.cend(); ++it) {
            if (joined.contains(it.key())) {
                done(0, it.value());
                return;
            }
        }
        done(0, {});
    }
    QStringList commands;
    bool hold = false;  // park every command until release()
    QList<std::function<void(int, const QByteArray &)>> held;
    void release()
    {
        while (!held.isEmpty()) held.takeFirst()(0, {});
    }
    QStringList failing;
    QMap<QString, QByteArray> outputs;
};

QJsonObject runSetup(PhoneSetup &setup, const QString &serial)
{
    QJsonObject report;
    setup.run(serial, [&report](const QJsonObject &r) { report = r; });
    return report;
}

} // namespace

class LinkSetupTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void runsEveryGrantForTheQindaLinkPackageOnly()
    {
        FakeAdb adb;
        PhoneSetup setup(adb);
        const QJsonObject report = runSetup(setup, QStringLiteral("R3GL40ABQXM"));
        QVERIFY(report.value(QStringLiteral("ok")).toBool());
        QCOMPARE(report.value(QStringLiteral("steps")).toArray().size(), 9);
        QCOMPARE(adb.commands.size(), 9);
        for (const QString &c : std::as_const(adb.commands)) {
            QVERIFY(c.startsWith(QStringLiteral("-s R3GL40ABQXM shell ")));
            QVERIFY(c.contains(QStringLiteral("org.qindaqt.link")));
        }
        QVERIFY(adb.commands.contains(
            QStringLiteral("-s R3GL40ABQXM shell pm grant org.qindaqt.link android.permission.SEND_SMS")));
        QVERIFY(adb.commands.contains(QStringLiteral(
            "-s R3GL40ABQXM shell appops set org.qindaqt.link ACCESS_RESTRICTED_SETTINGS allow")));
        QVERIFY(adb.commands.contains(QStringLiteral(
            "-s R3GL40ABQXM shell cmd notification allow_listener "
            "org.qindaqt.link/org.qindaqt.link.android.NotificationListener")));
        QVERIFY(adb.commands.contains(
            QStringLiteral("-s R3GL40ABQXM shell cmd deviceidle whitelist +org.qindaqt.link")));
    }

    void aFailedStepIsReportedAndTheRestStillRun()
    {
        FakeAdb adb;
        adb.failing = {QStringLiteral("READ_CONTACTS")};
        PhoneSetup setup(adb);
        const QJsonObject report = runSetup(setup, QStringLiteral("abc123"));
        QVERIFY(!report.value(QStringLiteral("ok")).toBool());
        const QJsonArray steps = report.value(QStringLiteral("steps")).toArray();
        QCOMPARE(steps.size(), 9);
        int failed = 0;
        for (const QJsonValue &s : steps) {
            failed += s.toObject().value(QStringLiteral("ok")).toBool() ? 0 : 1;
        }
        QCOMPARE(failed, 1);
        QCOMPARE(steps.at(3).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("grant:READ_CONTACTS"));
        QCOMPARE(steps.at(3).toObject().value(QStringLiteral("code")).toInt(), 1);
    }

    void aSecondSetupForTheSameSerialIsRefusedWhileOneRuns()
    {
        FakeAdb adb;
        adb.hold = true;
        PhoneSetup setup(adb);
        QJsonObject first;
        setup.run(QStringLiteral("abc"), [&first](const QJsonObject &r) { first = r; });
        QJsonObject second;
        setup.run(QStringLiteral("abc"), [&second](const QJsonObject &r) { second = r; });
        QCOMPARE(second.value(QStringLiteral("error")).toString(), QStringLiteral("busy"));
        QCOMPARE(adb.commands.size(), 1);  // the refused call ran nothing
        QJsonObject other;  // another phone is independent
        setup.run(QStringLiteral("xyz"), [&other](const QJsonObject &r) { other = r; });
        QCOMPARE(adb.commands.size(), 2);
        adb.hold = false;
        while (!adb.held.isEmpty()) adb.release();
        QVERIFY(first.value(QStringLiteral("ok")).toBool());
        QJsonObject again;
        setup.run(QStringLiteral("abc"), [&again](const QJsonObject &r) { again = r; });  // free again
        QVERIFY(again.value(QStringLiteral("ok")).toBool());
    }

    void refusesAnUnsafeSerialWithoutRunningAdb()
    {
        FakeAdb adb;
        PhoneSetup setup(adb);
        for (const QString &bad : {QString(), QStringLiteral("a b"), QStringLiteral("x;rm -rf"), QStringLiteral("$(id)"),
                                   QString(70, QLatin1Char('a')), QStringLiteral("abc\n")}) {
            const QJsonObject report = runSetup(setup, bad);
            QVERIFY(!report.value(QStringLiteral("ok")).toBool());
            QCOMPARE(report.value(QStringLiteral("error")).toString(), QStringLiteral("bad-serial"));
        }
        QVERIFY(adb.commands.isEmpty());
    }

    void statusReadsWithoutChangingAnything()
    {
        FakeAdb adb;
        QString dump;
        for (const char *p : {"READ_SMS", "SEND_SMS", "RECEIVE_SMS", "READ_CONTACTS", "POST_NOTIFICATIONS"}) {
            dump += QStringLiteral("    android.permission.%1: granted=true\n").arg(QLatin1String(p));
        }
        dump += QStringLiteral("    android.permission.READ_PHONE_STATE: granted=false\n");
        adb.outputs.insert(QStringLiteral("dumpsys package"), dump.toUtf8());
        adb.outputs.insert(QStringLiteral("appops get"), "ACCESS_RESTRICTED_SETTINGS: allow\n");
        adb.outputs.insert(QStringLiteral("enabled_notification_listeners"),
                           "org.qindaqt.link/org.qindaqt.link.android.NotificationListener\n");
        QJsonObject report;
        PhoneSetup setup(adb);
        setup.status(QStringLiteral("abc123"), [&report](const QJsonObject &r) { report = r; });
        const QJsonArray steps = report.value(QStringLiteral("steps")).toArray();
        QCOMPARE(steps.size(), 9);
        QVERIFY(!report.value(QStringLiteral("ok")).toBool());
        QStringList notOk;
        for (const QJsonValue &s : steps) {
            if (!s.toObject().value(QStringLiteral("ok")).toBool()) notOk << s.toObject().value(QStringLiteral("id")).toString();
        }
        QCOMPARE(notOk, (QStringList{QStringLiteral("grant:READ_PHONE_STATE"), QStringLiteral("battery-whitelist")}));
        for (const QString &c : std::as_const(adb.commands)) {
            QVERIFY(!c.contains(QStringLiteral(" grant ")) && !c.contains(QStringLiteral("appops set"))
                    && !c.contains(QStringLiteral("allow_listener")) && !c.contains(QStringLiteral("whitelist +")));
        }
    }
};

QTEST_GUILESS_MAIN(LinkSetupTest)
#include "tst_link_setup.moc"
