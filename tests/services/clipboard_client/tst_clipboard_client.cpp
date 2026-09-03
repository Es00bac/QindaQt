// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_clipboard_transport.h"

#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Services;

namespace {
Clipboard::Snapshot snapshot(quint64 epoch, quint32 generation, quint64 revision)
{
    const auto encoded = ClipboardModel::encodeDescriptorList({});
    return {.epoch = epoch, .generation = generation, .revision = revision,
            .historyEnabled = true, .privacyAllowed = true,
            .descriptorList = encoded.bytes};
}
}

class ClipboardClientTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void exactOwnerAndLineage()
    {
        FakeClipboardTransport transport;
        Clipboard::ClipboardClient client(&transport);
        QSignalSpy snapshots(&client, &Clipboard::ClipboardClient::snapshotChanged);
        client.start();
        transport.announceOwner(QStringLiteral(":1.4"));
        const quint64 token = transport.lastFetchToken;
        transport.replySnapshot(QStringLiteral(":1.3"), token, snapshot(4, 1, 0));
        QVERIFY(!client.hasSnapshot());
        transport.replySnapshot(QStringLiteral(":1.4"), token, snapshot(4, 1, 0));
        QCOMPARE(snapshots.count(), 1);
        QVERIFY(client.hasSnapshot());
        Q_EMIT transport.invalidated(QStringLiteral(":1.4"), 4, 1, 1);
        QVERIFY(transport.lastFetchToken != token);
    }

    void operationFencesReply()
    {
        FakeClipboardTransport transport;
        Clipboard::ClipboardClient client(&transport);
        QSignalSpy completed(&client, &Clipboard::ClipboardClient::operationCompleted);
        client.start();
        transport.announceOwner(QStringLiteral(":1.7"));
        transport.replySnapshot(QStringLiteral(":1.7"), transport.lastFetchToken,
                                snapshot(8, 2, 3));
        const quint64 requestId = client.clear(true);
        Clipboard::OperationResult stale{.kind = Clipboard::OperationKind::Clear,
            .status = Clipboard::OperationStatus::Succeeded, .requestId = requestId,
            .initiatingEpoch = 7, .initiatingGeneration = 2,
            .initiatingRevision = 3, .observedEpoch = 7,
            .observedGeneration = 2, .observedRevision = 4,
            .reasonCode = QStringLiteral("ok")};
        transport.replyOperation(QStringLiteral(":1.7"), requestId, stale);
        QTRY_COMPARE(completed.count(), 1);
        const auto result = qvariant_cast<Clipboard::OperationResult>(completed.at(0).at(1));
        QCOMPARE(result.status, Clipboard::OperationStatus::Uncertain);
    }
};

QTEST_GUILESS_MAIN(ClipboardClientTest)
#include "tst_clipboard_client.moc"
