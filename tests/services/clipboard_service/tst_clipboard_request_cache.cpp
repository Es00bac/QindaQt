// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_wayland_adapter.h"
#include "support/private_bus.h"

#include <qindaqt/services/clipboard_client/qt_clipboard_transport.h>
#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <memory>
#include <vector>

using namespace QindaQt::Services;

namespace {

Clipboard::OperationRequest clearRequest(const Clipboard::Snapshot &snapshot,
                                         quint64 requestId)
{
    return {.kind = Clipboard::OperationKind::Clear,
            .requestId = requestId,
            .expectedEpoch = snapshot.epoch,
            .expectedGeneration = snapshot.generation,
            .expectedRevision = snapshot.revision,
            .entry = {},
            .clearAll = true};
}

} // namespace

class ClipboardRequestCacheTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void evictsOldestResultsWithoutBlockingFreshIds()
    {
        QindaQt::Tests::PrivateClipboardBus bus;
        QVERIFY(bus.start());
        auto adapter = std::make_unique<FakeWaylandAdapter>();
        const QString serviceName = QStringLiteral("org.qindaqt.Clipboard1.CacheTest");
        Clipboard::ResidentClipboardService service(
            std::move(adapter), bus.serviceConnection, serviceName, 301);
        QCOMPARE(service.start(), Clipboard::ServiceStartStatus::Started);
        service.host()->setHistoryOptIn(true);
        service.host()->setUnlocked(true);

        QDBusConnection connection = bus.connectClient(QStringLiteral("eviction"));
        Clipboard::QtClipboardTransport transport(connection, serviceName);
        QSignalSpy owners(&transport, &Clipboard::ClipboardTransport::ownerChanged);
        QSignalSpy replies(&transport, &Clipboard::ClipboardTransport::operationReply);
        transport.start();
        QTRY_COMPARE(owners.count(), 1);
        const QString owner = owners.constFirst().constFirst().toString();
        QVERIFY(!owner.isEmpty());

        const Clipboard::Snapshot snapshot = service.host()->snapshot();
        Clipboard::OperationResult newest;
        const quint64 requestCount =
            static_cast<quint64>(Clipboard::kMaxRememberedRequestsPerCaller) + 2;
        for (quint64 requestId = 1; requestId <= requestCount; ++requestId) {
            transport.submitOperation(owner, requestId, clearRequest(snapshot, requestId));
            QTRY_COMPARE(replies.count(), static_cast<qsizetype>(requestId));
            const auto result = qvariant_cast<Clipboard::OperationResult>(
                replies.constLast().at(3));
            QCOMPARE(result.status, Clipboard::OperationStatus::Succeeded);
            if (requestId == requestCount) {
                newest = result;
            }
        }

        transport.submitOperation(owner, requestCount + 1,
                                  clearRequest(snapshot, requestCount));
        QTRY_COMPARE(replies.count(), static_cast<qsizetype>(requestCount + 1));
        QCOMPARE(qvariant_cast<Clipboard::OperationResult>(replies.constLast().at(3)),
                 newest);
    }

    void rejectsTheSixtyFifthConcurrentCaller()
    {
        QindaQt::Tests::PrivateClipboardBus bus;
        QVERIFY(bus.start());
        auto adapter = std::make_unique<FakeWaylandAdapter>();
        const QString serviceName = QStringLiteral("org.qindaqt.Clipboard1.CallerCapTest");
        Clipboard::ResidentClipboardService service(
            std::move(adapter), bus.serviceConnection, serviceName, 302);
        QCOMPARE(service.start(), Clipboard::ServiceStartStatus::Started);
        service.host()->setHistoryOptIn(true);
        service.host()->setUnlocked(true);
        const Clipboard::Snapshot snapshot = service.host()->snapshot();

        std::vector<std::unique_ptr<Clipboard::QtClipboardTransport>> transports;
        transports.reserve(static_cast<size_t>(Clipboard::kMaxRememberedCallers + 1));
        for (qsizetype index = 0; index <= Clipboard::kMaxRememberedCallers; ++index) {
            QDBusConnection connection = bus.connectClient(QString::number(index));
            auto transport = std::make_unique<Clipboard::QtClipboardTransport>(
                connection, serviceName);
            QSignalSpy owners(transport.get(), &Clipboard::ClipboardTransport::ownerChanged);
            QSignalSpy replies(transport.get(), &Clipboard::ClipboardTransport::operationReply);
            transport->start();
            QTRY_COMPARE(owners.count(), 1);
            const QString owner = owners.constFirst().constFirst().toString();
            QVERIFY(!owner.isEmpty());
            transport->submitOperation(owner, 1, clearRequest(snapshot, 1));
            QTRY_COMPARE(replies.count(), 1);
            const auto result = qvariant_cast<Clipboard::OperationResult>(
                replies.constFirst().at(3));
            if (index < Clipboard::kMaxRememberedCallers) {
                QCOMPARE(result.status, Clipboard::OperationStatus::Succeeded);
            } else {
                QCOMPARE(result.status, Clipboard::OperationStatus::Busy);
                QCOMPARE(result.reasonCode, QStringLiteral("caller-cache-full"));
            }
            transports.push_back(std::move(transport));
        }
    }
};

QTEST_GUILESS_MAIN(ClipboardRequestCacheTest)
#include "tst_clipboard_request_cache.moc"
