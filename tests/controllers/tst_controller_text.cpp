// SPDX-License-Identifier: GPL-3.0-or-later
#include "text_delivery_p.h"
#include <QtTest>
using namespace QindaQt::Controllers;
class Output final : public TextOutput {
public:
    QObject first, second; QObject *focus = &first;
    bool permitted = true, succeeds = true;
    int chunk = 1; QString result;
    QObject *target() const override { return focus; }
    bool allowed() const override { return permitted; }
    int chunkCodepoints() const override { return chunk; }
    bool insert(const QString &text) override { if (succeeds) result += text; return succeeds; }
};
class ControllerTextTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void unicodeNewlinesAndTabsSurviveBatchedDelivery() {
        Output output; TextDelivery delivery(output);
        const auto text = QString::fromUtf8("Hello\t世界 🌎\nSecond line\n");
        delivery.start("pad/1/button", text); QTRY_COMPARE(output.result, text);
        output.result.clear(); output.chunk = 128;
        delivery.start("pad/1/button", text.repeated(40)); QTRY_COMPARE(output.result, text.repeated(40));
    }
    void focusChangeAndSuppressionDropPendingText() {
        Output output; TextDelivery delivery(output); QSignalSpy failed(&delivery, &TextDelivery::failed);
        delivery.start("pad/1/button", "old"); output.focus = &output.second;
        QTest::qWait(15); QVERIFY(output.result.isEmpty());
        delivery.start("pad/1/button", "paused"); output.permitted = false;
        QTest::qWait(15); QVERIFY(output.result.isEmpty()); output.permitted = true;
        QTest::qWait(15); QVERIFY(output.result.isEmpty()); QCOMPARE(failed.size(), 0);
    }
    void cancellationMatchesControllerAndSupersessionNeverReplays() {
        Output output; TextDelivery delivery(output);
        delivery.start("pad/1/button", "old"); delivery.start("pad/1/button", "new");
        delivery.cancel("other/1"); QTRY_COMPARE(output.result, "new");
        delivery.start("pad/1/button", "cancelled"); delivery.cancel("pad/1");
        QTest::qWait(15); QCOMPARE(output.result, "new");
        delivery.start("pad/1/button", "cancelled"); delivery.cancel();
        QTest::qWait(15); QCOMPARE(output.result, "new");
    }
    void refusedTargetAndInvalidTextEmitNoCharacters() {
        Output output; TextDelivery delivery(output); QSignalSpy failed(&delivery, &TextDelivery::failed);
        output.focus = nullptr; delivery.start("pad/1/button", "text"); QCOMPARE(failed.size(), 1);
        output.focus = &output.first; output.succeeds = false; delivery.start("pad/1/button", "text");
        QTRY_COMPARE(failed.size(), 2); QVERIFY(output.result.isEmpty());
        delivery.start("pad/1/button", QString(4097, 'x')); QCOMPARE(failed.size(), 3);
    }
};
QTEST_GUILESS_MAIN(ControllerTextTest)
#include "tst_controller_text.moc"
