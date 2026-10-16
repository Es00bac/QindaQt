// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0379 bus discipline: the shell-side client answers Sleep1.Changed from
// its watcher cache, coalesces bursts and never polls absent services. A
// dbus-monitor on the private broker counts what the client actually sends.
#include "session_actions_fakes.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>

using namespace QindaQt::Services::SessionActions;
using namespace SessionActionsFakes;

namespace {
class CallMonitor final {
public:
    bool start(const QString &sender)
    {
        m_sender = sender;
        process.start(QStringLiteral("dbus-monitor"),
                      {QStringLiteral("--session"), QStringLiteral("type='method_call'")});
        if (!process.waitForStarted(5'000)) {
            return false;
        }
        // The monitor announces itself (NameAcquired/NameLost) once attached.
        QElapsedTimer timer;
        timer.start();
        while (!m_output.contains("NameLost") && timer.elapsed() < 5'000) {
            process.waitForReadyRead(50);
            m_output += process.readAllStandardOutput();
        }
        return m_output.contains("NameLost");
    }
    // Calls from the client's connection naming member; drains new output.
    int count(const QString &member)
    {
        m_output += process.readAllStandardOutput();
        int calls = 0;
        const auto sender = QByteArray("sender=") + m_sender.toUtf8() + ' ';
        const auto name = QByteArray("member=") + member.toUtf8();
        for (const auto &line : m_output.split('\n')) {
            if (line.contains(sender) && line.trimmed().endsWith(name)) {
                ++calls;
            }
        }
        return calls;
    }
    ~CallMonitor()
    {
        process.kill();
        process.waitForFinished(5'000);
    }
    QProcess process;

private:
    QString m_sender;
    QByteArray m_output;
};
} // namespace

class SessionActionsBusDisciplineTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void changedBurstUsesCacheAndCoalesces();
};

void SessionActionsBusDisciplineTest::changedBurstUsesCacheAndCoalesces()
{
    PrivateServices services;
    services.sleep.hibernateAllowed = true;
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start();
    QTRY_VERIFY(client.canSuspend());
    QTRY_VERIFY(client.canHibernate());
    QTest::qWait(50);
    CallMonitor monitor;
    QVERIFY(monitor.start(services.clientBus.baseService()));
    const int baselineSuspend = services.sleep.canCount;
    const int baselineHibernate = services.sleep.canHibernateCount;

    // The live session emitted Sleep1.Changed eight times per second and the
    // shell answered each with about thirteen blocking GetNameOwner calls.
    for (int i = 0; i < 40; ++i) {
        Q_EMIT services.sleep.Changed();
    }
    QTRY_VERIFY(services.sleep.canCount > baselineSuspend);
    QTest::qWait(200);
    // One refresh, plus at most one follow-up for signals that arrived while
    // it was running.
    QVERIFY(services.sleep.canCount - baselineSuspend <= 2);
    QVERIFY(services.sleep.canHibernateCount - baselineHibernate <= 2);
    QTRY_VERIFY(monitor.count(QStringLiteral("CanSuspend")) >= 1);
    QCOMPARE(monitor.count(QStringLiteral("GetNameOwner")), 0);
    QCOMPARE(monitor.count(QStringLiteral("NameHasOwner")), 0);
    QCOMPARE(monitor.count(QStringLiteral("GetConnectionUnixUser")), 0);
    // ScreenPower1 is absent here: it is never polled.
    QCOMPARE(monitor.count(QStringLiteral("CanScreenOff")), 0);
    QVERIFY(client.canSuspend());

    // A request re-checks admission against the cached exact owner, still
    // without asking the daemon who owns Sleep1.
    QSignalSpy finished(&client, &SessionActionsClient::actionFinished);
    QVERIFY(client.requestSuspend());
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(services.sleep.suspendCount, 1);
    QCOMPARE(monitor.count(QStringLiteral("GetNameOwner")), 0);
}

QTEST_GUILESS_MAIN(SessionActionsBusDisciplineTest)
#include "tst_session_actions_bus_discipline.moc"
