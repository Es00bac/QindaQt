// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/bluez_harness.h"
#include <qindaqt/services/bluetooth_radio_helper/radio_power_port.h>
#include <QtTest/QTest>

using namespace QindaQt::Bluetooth;
using namespace QindaQt::BluetoothRadio;
using QindaQt::Tests::BluezHarness;
class Radio final : public RadioPowerPort {
public:
    quint64 observeAndUnblock(const QString &, const QString &path, const QString &,
        const QString &, std::function<bool()> current) override {
        ++calls; selectedPath = path; admission = std::move(current); return 1;
    }
    void cancel(quint64) override { ++cancels; }
    void complete(Disposition state, const QString &reason) {
        Q_EMIT finished(1, {QString(32, QLatin1Char('a')), state, reason});
    }
    int calls = 0, cancels = 0;
    QString selectedPath;
    std::function<bool()> admission;
};
class BluezPowerRecoveryTest final : public QObject {
    Q_OBJECT
private:
    static QString address() { return QStringLiteral("AA:BB:CC:00:11:22"); }
    static QString prepare(BluezHarness &harness) {
        const auto path = harness.fake->addAdapter(QStringLiteral("hci0"), address(),
                                                   QStringLiteral("fixture"), false);
        if (!harness.fake->takeOwnership()) return {};
        harness.model->start();
        return harness.waitReady() ? path : QString{};
    }
    static OperationSubmission enable(BluezHarness &harness) {
        return harness.model->submit({.kind = OperationKind::SetAdapterPower,
            .target = harness.snapshotAdapter().handle, .powered = true},
            QStringLiteral(":1.10"));
    }
private Q_SLOTS:
    void helperThenExactlyOnePowerCall() {
        Radio radio; BluezHarness h(7901, 60'000, &radio);
        QVERIFY(h.ready()); const auto path = prepare(h); QVERIFY(!path.isEmpty());
        QSignalSpy results(h.model.get(), &BluetoothModel::operationCompleted);
        const auto submission = enable(h); QVERIFY(submission.pending);
        QTRY_COMPARE(radio.calls, 1); QCOMPARE(radio.selectedPath, path);
        QCOMPARE(h.fake->powerCalls, 0); QVERIFY(radio.admission());
        radio.complete(Disposition::VerifiedUnblocked, QStringLiteral("radio-unblocked"));
        const auto result = h.awaitResult(results, submission.operationId);
        QVERIFY(result); QCOMPARE(result->status, OperationStatus::Succeeded);
        QCOMPARE(h.fake->powerCalls, 1);
        QTRY_VERIFY(h.snapshotAdapter().powered);
    }
    void helperRefusalDoesNotPower_data() {
        QTest::addColumn<Disposition>("state"); QTest::addColumn<QString>("reason");
        QTest::newRow("hardware") << Disposition::Refused << QStringLiteral("radio-hardware-blocked");
        QTest::newRow("authority") << Disposition::Refused << QStringLiteral("radio-not-authorized");
        QTest::newRow("possible-write") << Disposition::Uncertain << QStringLiteral("radio-change-uncertain");
    }
    void helperRefusalDoesNotPower() {
        QFETCH(Disposition, state); QFETCH(QString, reason);
        Radio radio; BluezHarness h(7902, 60'000, &radio);
        QVERIFY(h.ready()); QVERIFY(!prepare(h).isEmpty());
        QSignalSpy results(h.model.get(), &BluetoothModel::operationCompleted);
        const auto submission = enable(h); QTRY_COMPARE(radio.calls, 1);
        radio.complete(state, reason);
        const auto result = h.awaitResult(results, submission.operationId);
        QVERIFY(result); QCOMPARE(result->reasonCode, reason);
        QCOMPARE(h.fake->powerCalls, 0); QVERIFY(!h.snapshotAdapter().powered);
    }
    void definitiveNoWriteRetainsDirectCompatibility() {
        Radio radio; BluezHarness h(7903, 60'000, &radio);
        QVERIFY(h.ready()); QVERIFY(!prepare(h).isEmpty());
        QSignalSpy results(h.model.get(), &BluetoothModel::operationCompleted);
        const auto submission = enable(h); QTRY_COMPARE(radio.calls, 1);
        radio.complete(Disposition::NoWriteUnavailable, QStringLiteral("radio-helper-unavailable"));
        QVERIFY(h.awaitResult(results, submission.operationId)); QCOMPARE(h.fake->powerCalls, 1);
    }
    void helperReplyAfterCallerLossCannotPower() {
        Radio radio; BluezHarness h(7904, 60'000, &radio);
        QVERIFY(h.ready()); QVERIFY(!prepare(h).isEmpty());
        (void)enable(h); QTRY_COMPARE(radio.calls, 1);
        h.backend->releaseOwner(QStringLiteral(":1.10"));
        QVERIFY(!radio.admission()); QCOMPARE(radio.cancels, 1);
        radio.complete(Disposition::VerifiedUnblocked, QStringLiteral("radio-unblocked"));
        QTest::qWait(20); QCOMPARE(h.fake->powerCalls, 0);
    }
    void helperReplyAfterSameAddressReplacementCannotPower() {
        Radio radio; BluezHarness h(7905, 60'000, &radio);
        QVERIFY(h.ready()); const auto path = prepare(h); QVERIFY(!path.isEmpty());
        (void)enable(h); QTRY_COMPARE(radio.calls, 1);
        h.fake->removeAdapterObject(path);
        QTRY_COMPARE(radio.cancels, 1);
        (void)h.fake->addAdapter(QStringLiteral("hci0"), address(), QStringLiteral("replacement"), false);
        QTRY_COMPARE(h.model->snapshot().adapters.size(), 1);
        QVERIFY(!radio.admission());
        radio.complete(Disposition::VerifiedUnblocked, QStringLiteral("radio-unblocked"));
        QTest::qWait(20); QCOMPARE(h.fake->powerCalls, 0); QVERIFY(!h.snapshotAdapter().powered);
    }
    void delayedBluezReplyCannotPowerReplacement() {
        BluezHarness h(7906); QVERIFY(h.ready()); const auto path = prepare(h);
        QVERIFY(!path.isEmpty()); h.fake->deferPower = true;
        QSignalSpy results(h.model.get(), &BluetoothModel::operationCompleted);
        const auto submission = enable(h); QTRY_COMPARE(h.fake->powerCalls, 1);
        h.fake->removeAdapterObject(path);
        QTRY_VERIFY(h.model->snapshot().adapters.isEmpty());
        (void)h.fake->addAdapter(QStringLiteral("hci0"), address(), QStringLiteral("replacement"), false);
        QTRY_COMPARE(h.model->snapshot().adapters.size(), 1);
        h.fake->replyDeferredPower(); QTest::qWait(20);
        QVERIFY(!h.snapshotAdapter().powered);
        const auto result = h.awaitResult(results, submission.operationId);
        QVERIFY(result); QVERIFY(result->status != OperationStatus::Succeeded);
    }
    void failedCallAndLaterPropertyAreIndependent() {
        BluezHarness h(7907); QVERIFY(h.ready()); const auto path = prepare(h);
        QVERIFY(!path.isEmpty()); h.fake->powerError = QStringLiteral("org.bluez.Error.Failed");
        QSignalSpy results(h.model.get(), &BluetoothModel::operationCompleted);
        const auto submission = enable(h);
        const auto result = h.awaitResult(results, submission.operationId);
        QVERIFY(result); QCOMPARE(result->status, OperationStatus::Uncertain);
        QCOMPARE(result->reasonCode, QStringLiteral("bluez-power-uncertain"));
        h.fake->setAdapterPowered(path, true); QTRY_VERIFY(h.snapshotAdapter().powered);
        QCOMPARE(h.fake->powerCalls, 1);
    }
    void successfulReplyDoesNotInventProperty() {
        BluezHarness h(7908); QVERIFY(h.ready()); QVERIFY(!prepare(h).isEmpty());
        h.fake->powerReplyOnly = true;
        QSignalSpy results(h.model.get(), &BluetoothModel::operationCompleted);
        const auto submission = enable(h);
        const auto result = h.awaitResult(results, submission.operationId);
        QVERIFY(result); QCOMPARE(result->status, OperationStatus::Succeeded);
        QVERIFY(!h.snapshotAdapter().powered);
    }
};
QTEST_GUILESS_MAIN(BluezPowerRecoveryTest)
#include "tst_bluez_power_recovery.moc"
