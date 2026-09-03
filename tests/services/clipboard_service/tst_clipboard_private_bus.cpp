// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_wayland_adapter.h"
#include "support/private_bus.h"

#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/clipboard_client/qt_clipboard_transport.h>
#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Services;

class ClipboardPrivateBusTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void clientServiceRoundTripAndOwnerLoss()
    {
        QindaQt::Tests::PrivateClipboardBus bus;
        QVERIFY(bus.start());
        auto adapter = std::make_unique<FakeWaylandAdapter>();
        FakeWaylandAdapter *adapterPointer = adapter.get();
        const QString serviceName = QStringLiteral("org.qindaqt.Clipboard1.Test");
        Clipboard::ResidentClipboardService service(
            std::move(adapter), bus.serviceConnection, serviceName, 101);
        QCOMPARE(service.start(), Clipboard::ServiceStartStatus::Started);
        service.host()->setHistoryOptIn(true);
        service.host()->setUnlocked(true);
        adapterPointer->offer({{{QStringLiteral("text/plain"), QByteArrayLiteral("bus fixture")}}});

        QDBusConnection clientBus = bus.connectClient(QStringLiteral("roundtrip"));
        QVERIFY(clientBus.isConnected());
        Clipboard::QtClipboardTransport transport(clientBus, serviceName);
        Clipboard::ClipboardClient client(&transport);
        QSignalSpy snapshots(&client, &Clipboard::ClipboardClient::snapshotChanged);
        QSignalSpy operations(&client, &Clipboard::ClipboardClient::operationCompleted);
        QSignalSpy transportOperations(&transport, &Clipboard::ClipboardTransport::operationReply);
        client.start();
        QTRY_VERIFY(client.hasSnapshot());
        QVERIFY(snapshots.count() >= 1);
        QCOMPARE(client.snapshot().epoch, quint64(101));

        const Clipboard::Snapshot initiating = client.snapshot();
        Clipboard::OperationRequest exact{.kind = Clipboard::OperationKind::Clear,
                                          .requestId = 900,
                                          .expectedEpoch = initiating.epoch,
                                          .expectedGeneration = initiating.generation,
                                          .expectedRevision = initiating.revision,
                                          .entry = {},
                                          .clearAll = true};
        transport.submitOperation(client.owner(), 901, exact);
        QTRY_COMPARE(transportOperations.count(), 1);
        const auto first = qvariant_cast<Clipboard::OperationResult>(
            transportOperations.at(0).at(3));
        QCOMPARE(first.status, Clipboard::OperationStatus::Succeeded);
        transport.submitOperation(client.owner(), 902, exact);
        QTRY_COMPARE(transportOperations.count(), 2);
        QCOMPARE(qvariant_cast<Clipboard::OperationResult>(
                     transportOperations.at(1).at(3)), first);
        exact.clearAll = false;
        transport.submitOperation(client.owner(), 903, exact);
        QTRY_COMPARE(transportOperations.count(), 3);
        const auto conflict = qvariant_cast<Clipboard::OperationResult>(
            transportOperations.at(2).at(3));
        QCOMPARE(conflict.status, Clipboard::OperationStatus::Rejected);
        QCOMPARE(conflict.reasonCode, QStringLiteral("request-id-conflict"));
        QTRY_VERIFY(client.snapshot().revision >= first.observedRevision);

        const quint64 requestId = client.clear(true);
        QVERIFY(requestId != 0);
        QTRY_COMPARE(operations.count(), 1);
        const auto result = qvariant_cast<Clipboard::OperationResult>(operations.at(0).at(1));
        QCOMPARE(result.status, Clipboard::OperationStatus::Succeeded);
        QTRY_VERIFY(client.snapshot().revision >= result.observedRevision);
        service.stop();
        QTRY_COMPARE(client.state(), Clipboard::ClientState::Unavailable);
        QVERIFY(!client.hasSnapshot());
    }
};

QTEST_GUILESS_MAIN(ClipboardPrivateBusTest)
#include "tst_clipboard_private_bus.moc"
