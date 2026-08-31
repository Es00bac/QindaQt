// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationbinding.h"

#include <QKeySequence>
#include <QTest>

using QindaQt::Test::DesktopNotificationBinding;
using QindaQt::Test::desktopNotificationActionId;
using QindaQt::Test::desktopNotificationBindingReady;
using QindaQt::Test::desktopNotificationMetaN;

class DesktopNotificationBindingTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void exactBindingIsReady();
    void incompleteOrDifferentBindingIsNotReady_data();
    void incompleteOrDifferentBindingIsNotReady();
};

void DesktopNotificationBindingTests::exactBindingIsReady()
{
    const int metaN = desktopNotificationMetaN();
    QVERIFY(desktopNotificationBindingReady(
        {desktopNotificationActionId(), {metaN}, {metaN}}));
}

void DesktopNotificationBindingTests::
    incompleteOrDifferentBindingIsNotReady_data()
{
    QTest::addColumn<QString>("actionId");
    QTest::addColumn<QList<int>>("defaultKeys");
    QTest::addColumn<QList<int>>("activeKeys");

    const int metaN = desktopNotificationMetaN();
    const int metaShiftN =
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_N)[0].toCombined();
    QTest::newRow("different-action")
        << QStringLiteral("another_action") << QList<int>{metaN}
        << QList<int>{metaN};
    QTest::newRow("default-not-published")
        << desktopNotificationActionId() << QList<int>{} << QList<int>{metaN};
    QTest::newRow("active-not-published")
        << desktopNotificationActionId() << QList<int>{metaN} << QList<int>{};
    QTest::newRow("active-remapped")
        << desktopNotificationActionId() << QList<int>{metaN}
        << QList<int>{metaShiftN};
}

void DesktopNotificationBindingTests::
    incompleteOrDifferentBindingIsNotReady()
{
    QFETCH(QString, actionId);
    QFETCH(QList<int>, defaultKeys);
    QFETCH(QList<int>, activeKeys);

    QVERIFY(!desktopNotificationBindingReady(
        DesktopNotificationBinding{actionId, defaultKeys, activeKeys}));
}

QTEST_GUILESS_MAIN(DesktopNotificationBindingTests)

#include "tst_desktopnotificationbinding.moc"
