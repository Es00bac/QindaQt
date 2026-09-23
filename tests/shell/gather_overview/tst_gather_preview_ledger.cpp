// SPDX-License-Identifier: GPL-3.0-or-later
#include "gatherpreviewledger.h"

#include <QtTest>

using QindaQt::Shell::GatherPreviewLedger;

class GatherPreviewLedgerTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void taskRevisionChurnKeepsTheSameWindowPreview();
    void aDepartedWindowIsDroppedWithoutFlashingSurvivors();
    void closingClearsEveryPreviewAndRequest();
};

void GatherPreviewLedgerTests::taskRevisionChurnKeepsTheSameWindowPreview()
{
    GatherPreviewLedger ledger;
    QVERIFY(ledger.reconcile({QStringLiteral("window-a")}));
    QVERIFY(ledger.markRequested(QStringLiteral("window-a")));
    // The task-list generation may change before an asynchronous capture
    // returns. An unchanged window identity still owns that result.
    QVERIFY(!ledger.reconcile({QStringLiteral("window-a")}));
    QVERIFY(ledger.accept(QStringLiteral("window-a"), QStringLiteral("image-a")));
    QCOMPARE(ledger.urls().value(QStringLiteral("window-a")).toString(),
             QStringLiteral("image-a"));
    for (int revision = 0; revision < 20; ++revision) {
        QVERIFY(!ledger.reconcile({QStringLiteral("window-a")}));
        QVERIFY(!ledger.markRequested(QStringLiteral("window-a")));
        QCOMPARE(ledger.urls().value(QStringLiteral("window-a")).toString(),
                 QStringLiteral("image-a"));
    }
}

void GatherPreviewLedgerTests::aDepartedWindowIsDroppedWithoutFlashingSurvivors()
{
    GatherPreviewLedger ledger;
    QVERIFY(ledger.reconcile({QStringLiteral("a"), QStringLiteral("b")}));
    QVERIFY(ledger.markRequested(QStringLiteral("a")));
    QVERIFY(ledger.markRequested(QStringLiteral("b")));
    QVERIFY(ledger.accept(QStringLiteral("a"), QStringLiteral("image-a")));
    QVERIFY(ledger.accept(QStringLiteral("b"), QStringLiteral("image-b")));

    QVERIFY(ledger.reconcile({QStringLiteral("b"), QStringLiteral("c")}));
    QVERIFY(!ledger.urls().contains(QStringLiteral("a")));
    QCOMPARE(ledger.urls().value(QStringLiteral("b")).toString(),
             QStringLiteral("image-b"));
    QVERIFY(!ledger.accept(QStringLiteral("a"), QStringLiteral("late-a")));
    QVERIFY(!ledger.markRequested(QStringLiteral("b")));
    QVERIFY(ledger.markRequested(QStringLiteral("c")));
    QVERIFY(ledger.accept(QStringLiteral("c"), QStringLiteral("image-c")));
}

void GatherPreviewLedgerTests::closingClearsEveryPreviewAndRequest()
{
    GatherPreviewLedger ledger;
    QVERIFY(ledger.reconcile({QStringLiteral("a")}));
    QVERIFY(ledger.markRequested(QStringLiteral("a")));
    QVERIFY(ledger.accept(QStringLiteral("a"), QStringLiteral("image-a")));
    ledger.clear();
    QVERIFY(ledger.urls().isEmpty());
    QVERIFY(!ledger.accept(QStringLiteral("a"), QStringLiteral("late-a")));
    QVERIFY(ledger.reconcile({QStringLiteral("a")}));
    QVERIFY(ledger.markRequested(QStringLiteral("a")));
}

QTEST_GUILESS_MAIN(GatherPreviewLedgerTests)
#include "tst_gather_preview_ledger.moc"
