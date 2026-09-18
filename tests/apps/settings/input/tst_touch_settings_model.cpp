// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/touch_settings_model.h>
#include "support/fake_settings_transport.h"

#include <QSignalSpy>
#include <QTest>

namespace QindaQt::Apps::SettingsInput {
namespace {

using Services::SettingsClient::SettingsClient;
using QindaQt::Tests::FakeSettingsTransport;
using QindaQt::Tests::fakeCommitWire;
using QindaQt::Tests::fakeSnapshotWire;
using QindaQt::Tests::withTouchDefaults;
using Services::SettingsProtocol::SettingsWireStatus;

struct Harness {
    FakeSettingsTransport transport;
    SettingsClient client{transport, TouchSettingsModel::settingsKeys()};
    TouchSettingsModel model{client};
    const QString owner = QStringLiteral(":1.7");

    // The client asks for its snapshot on the next event-loop turn after the
    // owner appears; the reply then makes the model available.
    [[nodiscard]] bool deliverSnapshot(quint64 revision, const QVariantMap &values)
    {
        Q_EMIT transport.ownerChanged(owner);
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); }, 2000)) {
            return false;
        }
        Q_EMIT transport.snapshotReceived(transport.snapshots.constLast().token, owner,
                                          fakeSnapshotWire(revision, withTouchDefaults(values)));
        return QTest::qWaitFor([this] { return model.available(); }, 2000);
    }

    // After every commit reply the client re-reads its scope; the next write
    // is allowed only once that snapshot has arrived.
    [[nodiscard]] bool answerRefresh(quint64 revision, const QVariantMap &values)
    {
        const qsizetype before = transport.snapshots.size();
        if (!QTest::qWaitFor([this, before] { return transport.snapshots.size() > before; }, 2000)) {
            return false;
        }
        Q_EMIT transport.snapshotReceived(transport.snapshots.constLast().token, owner,
                                          fakeSnapshotWire(revision, withTouchDefaults(values)));
        return QTest::qWaitFor([this] { return !model.busy() && model.available(); }, 2000);
    }
};

} // namespace

class TouchSettingsModelTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultsBeforeAnySnapshot();
    void refreshStartsTheScopedClientOnce();
    void snapshotDrivesEveryRow();
    void editsWriteOneUserValueAndReconcile();
    void invalidEditsAreRefusedWithoutAWrite();
    void rejectedCommitReportsAndClearsBusy();
};

void TouchSettingsModelTests::defaultsBeforeAnySnapshot()
{
    Harness harness;
    QVERIFY(!harness.model.available());
    QVERIFY(harness.model.touchscreenEnabled());
    QCOMPARE(harness.model.longPressMs(), 500);
    QCOMPARE(harness.model.onScreenKeyboard(), QStringLiteral("auto"));
    QCOMPARE(harness.model.edgeLeft(), QStringLiteral("overview"));
    QCOMPARE(harness.model.edgeTop(), QStringLiteral("notifications"));
    QCOMPARE(harness.model.edgeRight(), QStringLiteral("none"));
    QCOMPARE(harness.model.edgeBottom(), QStringLiteral("task-switcher"));
    QCOMPARE(harness.model.keyboardChoices().size(), 2);
    QCOMPARE(harness.model.edgeActionChoices().size(), 4);
    QCOMPARE(harness.model.choiceIndex(harness.model.edgeActionChoices(), QStringLiteral("task-switcher")), 3);
    QCOMPARE(harness.model.choiceIndex(harness.model.edgeActionChoices(), QStringLiteral("bogus")), 0);
    QCOMPARE(TouchSettingsModel::settingsKeys().size(), 7);
    QVERIFY(!harness.model.statusText().isEmpty());
}

void TouchSettingsModelTests::refreshStartsTheScopedClientOnce()
{
    Harness harness;
    harness.model.refresh();
    harness.model.refresh();
    QCOMPARE(harness.transport.starts, 1);
    QVERIFY(!harness.transport.snapshots.isEmpty() || harness.transport.activations > 0
            || harness.transport.starts == 1);
    Harness broken;
    broken.transport.startSucceeds = false;
    broken.model.refresh();
    QVERIFY(broken.model.errorText().contains(QStringLiteral("unavailable")));
}

void TouchSettingsModelTests::snapshotDrivesEveryRow()
{
    Harness harness;
    harness.model.refresh();
    QSignalSpy changed(&harness.model, &TouchSettingsModel::changed);
    QVERIFY(harness.deliverSnapshot(4, {{QStringLiteral("input.touch.enabled"), false},
                                {QStringLiteral("input.touch.longPressMs"), 750},
                                {QStringLiteral("input.touch.onScreenKeyboard"), QStringLiteral("off")},
                                {QStringLiteral("input.touch.edgeLeft"), QStringLiteral("none")},
                                {QStringLiteral("input.touch.edgeRight"), QStringLiteral("notifications")}}));
    QVERIFY(harness.model.available());
    QVERIFY(!harness.model.touchscreenEnabled());
    QCOMPARE(harness.model.longPressMs(), 750);
    QCOMPARE(harness.model.onScreenKeyboard(), QStringLiteral("off"));
    QCOMPARE(harness.model.edgeLeft(), QStringLiteral("none"));
    QCOMPARE(harness.model.edgeRight(), QStringLiteral("notifications"));
    QCOMPARE(harness.model.edgeTop(), QStringLiteral("notifications"));
    QCOMPARE(harness.model.statusText(), QStringLiteral("The touchscreen is off."));
    QVERIFY(changed.count() >= 1);
}

void TouchSettingsModelTests::editsWriteOneUserValueAndReconcile()
{
    Harness harness;
    harness.model.refresh();
    QVERIFY(harness.deliverSnapshot(1, {}));
    QVERIFY(harness.model.setLongPressMs(900));
    QVERIFY(harness.model.busy());
    QCOMPARE(harness.transport.commits.size(), 1);
    QCOMPARE(harness.transport.commits.constFirst().operations.size(), 1);
    const QVariantMap operation = harness.transport.commits.constFirst().operations.constFirst().toMap();
    QCOMPARE(operation.value(QStringLiteral("key")).toString(), QStringLiteral("input.touch.longPressMs"));
    // A second edit while one is in flight is refused, not queued.
    QVERIFY(!harness.model.setEdgeAction(QStringLiteral("left"), QStringLiteral("none")));
    Q_EMIT harness.transport.commitReceived(
        harness.transport.commits.constFirst().token, harness.owner,
        fakeCommitWire(SettingsWireStatus::Applied, 1, 2, {{QStringLiteral("input.touch.longPressMs"), 900}}));
    QTRY_VERIFY(!harness.model.busy());
    QVERIFY(harness.model.errorText().isEmpty());
    QVERIFY(harness.answerRefresh(2, {{QStringLiteral("input.touch.longPressMs"), 900}}));
    QCOMPARE(harness.model.longPressMs(), 900);
    QVERIFY(harness.model.setEdgeAction(QStringLiteral("left"), QStringLiteral("none")));
    QCOMPARE(harness.transport.commits.size(), 2);
    QCOMPARE(harness.transport.commits.at(1).operations.constFirst().toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("input.touch.edgeLeft"));
}

void TouchSettingsModelTests::invalidEditsAreRefusedWithoutAWrite()
{
    Harness harness;
    harness.model.refresh();
    QVERIFY(harness.deliverSnapshot(1, {}));
    QVERIFY(!harness.model.setLongPressMs(10));
    QVERIFY(harness.model.errorText().contains(QStringLiteral("200")));
    QVERIFY(!harness.model.setOnScreenKeyboard(QStringLiteral("maybe")));
    QVERIFY(!harness.model.setOnScreenKeyboard(QStringLiteral("on")));
    QVERIFY(!harness.model.setEdgeAction(QStringLiteral("diagonal"), QStringLiteral("overview")));
    QVERIFY(!harness.model.setEdgeAction(QStringLiteral("left"), QStringLiteral("launch-missiles")));
    QVERIFY(harness.transport.commits.isEmpty());
    QVERIFY(!harness.model.busy());
}

void TouchSettingsModelTests::rejectedCommitReportsAndClearsBusy()
{
    Harness harness;
    harness.model.refresh();
    QVERIFY(harness.deliverSnapshot(1, {}));
    QVERIFY(harness.model.setTouchscreenEnabled(false));
    Q_EMIT harness.transport.commitReceived(
        harness.transport.commits.constFirst().token, harness.owner,
        fakeCommitWire(SettingsWireStatus::ValidationFailed, 1, 1, {}));
    QTRY_VERIFY(!harness.model.busy());
    // A rejected commit leaves the error on the row and the value unchanged.
    QVERIFY(!harness.model.errorText().isEmpty());
    QVERIFY(harness.model.touchscreenEnabled());
    // The client re-reads its scope after the commit; that confirmed snapshot
    // reconciles the row and clears the error.
    QVERIFY(harness.answerRefresh(1, {}));
    QVERIFY(harness.model.errorText().isEmpty());
    // A lost reply is different: the client reports an uncertain commit and the
    // row says "may not have been applied" instead of pretending.
    QVERIFY(harness.model.setTouchscreenEnabled(false));
    QVERIFY(!harness.transport.commits.isEmpty());
    Q_EMIT harness.transport.requestFailed(harness.transport.commits.constLast().token, harness.owner,
                                           QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                                           QStringLiteral("owner vanished"));
    QTRY_VERIFY(!harness.model.busy());
    QVERIFY(harness.model.errorText().contains(QStringLiteral("may not have been applied")));
}

} // namespace QindaQt::Apps::SettingsInput

QTEST_GUILESS_MAIN(QindaQt::Apps::SettingsInput::TouchSettingsModelTests)
#include "tst_touch_settings_model.moc"
