// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationbinding.h"

#include <QKeySequence>
#include <QTest>

using QindaQt::Test::DesktopNotificationBinding;
using QindaQt::Test::DesktopNotificationActivationEdge;
using QindaQt::Test::DesktopNotificationActivationEvent;
using QindaQt::Test::desktopNotificationActionId;
using QindaQt::Test::desktopNotificationActivationComplete;
using QindaQt::Test::desktopNotificationBindingReady;
using QindaQt::Test::desktopNotificationComponentId;
using QindaQt::Test::desktopNotificationMetaN;

class DesktopNotificationBindingTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void exactBindingIsReady();
    void incompleteOrDifferentBindingIsNotReady_data();
    void incompleteOrDifferentBindingIsNotReady();
    void exactActivationSequenceIsComplete();
    void malformedActivationSequencesAreIncomplete();
};

void DesktopNotificationBindingTests::exactBindingIsReady()
{
    const int metaN = desktopNotificationMetaN();
    QVERIFY(desktopNotificationBindingReady(
        {desktopNotificationComponentId(), desktopNotificationActionId(),
         QStringLiteral("/component/qindaqt_shell"), true, true, true,
         {metaN}, {metaN}}));
}

void DesktopNotificationBindingTests::
    incompleteOrDifferentBindingIsNotReady_data()
{
    QTest::addColumn<QString>("componentId");
    QTest::addColumn<QString>("actionId");
    QTest::addColumn<QString>("componentPath");
    QTest::addColumn<bool>("componentResolved");
    QTest::addColumn<bool>("componentActiveReplyValid");
    QTest::addColumn<bool>("componentActive");
    QTest::addColumn<QList<int>>("defaultKeys");
    QTest::addColumn<QList<int>>("activeKeys");

    const int metaN = desktopNotificationMetaN();
    const int metaShiftN =
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_N)[0].toCombined();
    const QString component = desktopNotificationComponentId();
    const QString action = desktopNotificationActionId();
    const QString path = QStringLiteral("/component/qindaqt_shell");
    QTest::newRow("different-component")
        << QStringLiteral("another-component") << action << path << true << true
        << true << QList<int>{metaN} << QList<int>{metaN};
    QTest::newRow("different-action")
        << component << QStringLiteral("another_action") << path << true << true
        << true << QList<int>{metaN} << QList<int>{metaN};
    QTest::newRow("component-unresolved")
        << component << action << QString{} << false << false << false
        << QList<int>{metaN} << QList<int>{metaN};
    QTest::newRow("component-active-query-error")
        << component << action << path << true << false << false
        << QList<int>{metaN} << QList<int>{metaN};
    QTest::newRow("component-inactive")
        << component << action << path << true << true << false
        << QList<int>{metaN} << QList<int>{metaN};
    QTest::newRow("default-not-published")
        << component << action << path << true << true << true << QList<int>{}
        << QList<int>{metaN};
    QTest::newRow("active-not-published")
        << component << action << path << true << true << true
        << QList<int>{metaN} << QList<int>{};
    QTest::newRow("active-remapped")
        << component << action << path << true << true << true
        << QList<int>{metaN} << QList<int>{metaShiftN};
}

void DesktopNotificationBindingTests::
    incompleteOrDifferentBindingIsNotReady()
{
    QFETCH(QString, componentId);
    QFETCH(QString, actionId);
    QFETCH(QString, componentPath);
    QFETCH(bool, componentResolved);
    QFETCH(bool, componentActiveReplyValid);
    QFETCH(bool, componentActive);
    QFETCH(QList<int>, defaultKeys);
    QFETCH(QList<int>, activeKeys);

    QVERIFY(!desktopNotificationBindingReady(
        DesktopNotificationBinding{
            componentId,
            actionId,
            componentPath,
            componentResolved,
            componentActiveReplyValid,
            componentActive,
            defaultKeys,
            activeKeys,
        }));
}

void DesktopNotificationBindingTests::exactActivationSequenceIsComplete()
{
    const QString component = desktopNotificationComponentId();
    const QString action = desktopNotificationActionId();
    QVERIFY(desktopNotificationActivationComplete(
        {{DesktopNotificationActivationEdge::Pressed, component, action},
         {DesktopNotificationActivationEdge::Released, component, action}}));
}

void DesktopNotificationBindingTests::malformedActivationSequencesAreIncomplete()
{
    const QString component = desktopNotificationComponentId();
    const QString action = desktopNotificationActionId();
    const DesktopNotificationActivationEvent pressed{
        DesktopNotificationActivationEdge::Pressed, component, action};
    const DesktopNotificationActivationEvent released{
        DesktopNotificationActivationEdge::Released, component, action};

    QVERIFY(!desktopNotificationActivationComplete({}));
    QVERIFY(!desktopNotificationActivationComplete({pressed}));
    QVERIFY(!desktopNotificationActivationComplete({released, pressed}));
    QVERIFY(!desktopNotificationActivationComplete(
        {{DesktopNotificationActivationEdge::Pressed,
          QStringLiteral("another-component"), action},
         released}));
    QVERIFY(!desktopNotificationActivationComplete(
        {{DesktopNotificationActivationEdge::Pressed, component,
          QStringLiteral("another-action")},
         released}));
    QVERIFY(!desktopNotificationActivationComplete(
        {pressed,
         {DesktopNotificationActivationEdge::Released, component,
          QStringLiteral("another-action")}}));
}

QTEST_GUILESS_MAIN(DesktopNotificationBindingTests)

#include "tst_desktopnotificationbinding.moc"
