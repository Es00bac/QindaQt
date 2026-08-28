// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QtTest/QtTest>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

class TstClipboardAppletSeam : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testClientSeamContracts();
    void testFailClosedLocking();
    void testSearchThroughSeam();
};

void TstClipboardAppletSeam::testClientSeamContracts()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardModelClientAdapter adapter(&model);

    QCOMPARE(adapter.clientState(), ClientState::Ready);
    QCOMPARE(adapter.owner(), QStringLiteral("org.qindaqt.ClipboardService"));
    QCOMPARE(adapter.isOwnerAvailable(), true);
    QCOMPARE(adapter.isLocked(), false);

    // Verify snapshot passes metadata without exposing raw data
    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "Confidential data" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("Terminal"), 100);
    QVERIFY(admitted.accepted());

    const auto snapshot = adapter.snapshot();
    QCOMPARE(snapshot.entries.size(), 1);
    QCOMPARE(snapshot.entries.first().id, admitted.entry.id);
    QCOMPARE(snapshot.entries.first().preview, QStringLiteral("Confidential data"));

    // Pinning via seam
    quint64 reqId = adapter.requestSetPinned(admitted.entry.id, true, 1);
    QVERIFY(reqId > 0);

    const auto pinnedSnap = adapter.snapshot();
    QCOMPARE(pinnedSnap.entries.first().pinned, true);

    // Deletion via seam
    reqId = adapter.requestRemove(admitted.entry.id, 1);
    QVERIFY(reqId > 0);

    const auto emptySnap = adapter.snapshot();
    QCOMPARE(emptySnap.entries.size(), 0);
}

void TstClipboardAppletSeam::testFailClosedLocking()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val;
    val.formats = { { QStringLiteral("text/plain"), "Secret" } };
    const auto admitted = model.admit(val, 1, QStringLiteral("App"), 100);
    QVERIFY(admitted.accepted());

    ClipboardModelClientAdapter adapter(&model);

    // Lock
    adapter.setLocked(true);
    QCOMPARE(adapter.isLocked(), true);

    // Snapshot while locked must be empty
    const auto lockedSnap = adapter.snapshot();
    QVERIFY(lockedSnap.entries.isEmpty());
    QCOMPARE(lockedSnap.privacyAllowed, false);

    // Requests while locked must fail closed
    const quint64 reqId = adapter.requestPromote(admitted.entry.id, 1, 100);
    QVERIFY(reqId > 0);
}

void TstClipboardAppletSeam::testSearchThroughSeam()
{
    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue val1;
    val1.formats = { { QStringLiteral("text/plain"), "Target substring" } };
    const auto admitRes = model.admit(val1, 1, QStringLiteral("Editor"), 100);
    QVERIFY(admitRes.accepted());

    ClipboardModelClientAdapter adapter(&model);

    bool searchReceived = false;
    SearchOutcome receivedOutcome;
    connect(&adapter, &ClipboardClientInterface::searchCompleted,
            [&](quint64, const SearchOutcome &outcome) {
                searchReceived = true;
                receivedOutcome = outcome;
            });

    const quint64 reqId = adapter.requestSearch(QStringLiteral("Target"), 1, 10);
    QVERIFY(reqId > 0);
    QVERIFY(searchReceived);
    QVERIFY(receivedOutcome.accepted());
    QCOMPARE(receivedOutcome.matches.size(), 1);
    QCOMPARE(receivedOutcome.matches.first().preview, QStringLiteral("Target substring"));
}

QTEST_MAIN(TstClipboardAppletSeam)
#include "tst_clipboard_applet_seam.moc"
