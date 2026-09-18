// SPDX-License-Identifier: GPL-3.0-or-later
#include "edgegesturesubscriber.h"

#include <QTest>

using namespace QindaQt::Shell;

class EdgeGestureSubscriberTests final : public QObject {
    Q_OBJECT

private slots:
    void dispatchesKnownActionsToTheirHandlers();
    void unknownOrUnhandledActionsAreReported();
};

void EdgeGestureSubscriberTests::dispatchesKnownActionsToTheirHandlers()
{
    QStringList calls;
    const EdgeGestureHandlers handlers{
        .overview = [&calls] { calls.append(QStringLiteral("overview")); },
        .notifications = [&calls] { calls.append(QStringLiteral("notifications")); },
        .taskSwitcher = [&calls] { calls.append(QStringLiteral("task-switcher")); },
    };
    QVERIFY(dispatchEdgeGesture(QStringLiteral("overview"), handlers));
    QVERIFY(dispatchEdgeGesture(QStringLiteral("notifications"), handlers));
    QVERIFY(dispatchEdgeGesture(QStringLiteral("task-switcher"), handlers));
    QCOMPARE(calls, (QStringList{QStringLiteral("overview"), QStringLiteral("notifications"),
                                 QStringLiteral("task-switcher")}));
}

void EdgeGestureSubscriberTests::unknownOrUnhandledActionsAreReported()
{
    int overviewCalls = 0;
    const EdgeGestureHandlers handlers{.overview = [&overviewCalls] { ++overviewCalls; }};
    QVERIFY(!dispatchEdgeGesture(QStringLiteral("none"), handlers));
    QVERIFY(!dispatchEdgeGesture(QStringLiteral("launch-missiles"), handlers));
    QVERIFY(!dispatchEdgeGesture(QStringLiteral("notifications"), handlers));
    QVERIFY(dispatchEdgeGesture(QStringLiteral("overview"), handlers));
    QCOMPARE(overviewCalls, 1);
    // No session bus in the test environment: the subscriber reports that
    // instead of pretending, and a gesture is not dispatched.
    const QDBusConnection disconnected = QDBusConnection::connectToBus(QStringLiteral("unix:path=/nonexistent"),
                                                                       QStringLiteral("edge-gesture-test"));
    const EdgeGestureSubscriber subscriber(handlers, disconnected);
    QVERIFY(!subscriber.subscribed());
    QDBusConnection::disconnectFromBus(QStringLiteral("edge-gesture-test"));
}

QTEST_GUILESS_MAIN(EdgeGestureSubscriberTests)
#include "tst_edgegesturesubscriber.moc"
