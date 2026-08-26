// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellvisibilityrefreshscheduler.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class ShellVisibilityRefreshSchedulerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void coalescesOneEventLoopBurst();
    void preservesARequestRaisedDuringRefresh();
};

void ShellVisibilityRefreshSchedulerTest::coalescesOneEventLoopBurst()
{
    int calls = 0;
    ShellVisibilityRefreshScheduler scheduler([&calls] { ++calls; });
    for (int index = 0; index < 100; ++index) {
        scheduler.request();
    }
    QVERIFY(scheduler.pending());
    QTRY_COMPARE(calls, 1);
    QVERIFY(!scheduler.pending());
    QCoreApplication::processEvents();
    QCOMPARE(calls, 1);
}

void ShellVisibilityRefreshSchedulerTest::preservesARequestRaisedDuringRefresh()
{
    int calls = 0;
    ShellVisibilityRefreshScheduler *address = nullptr;
    ShellVisibilityRefreshScheduler scheduler([&] {
        ++calls;
        if (calls == 1) {
            address->request();
        }
    });
    address = &scheduler;
    scheduler.request();
    QTRY_COMPARE(calls, 2);
    QVERIFY(!scheduler.pending());
}

QTEST_GUILESS_MAIN(ShellVisibilityRefreshSchedulerTest)

#include "tst_shellvisibilityrefreshscheduler.moc"
