// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/reentrant_wayland_adapter.h"

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/clipboard_service/clipboard_host.h>

#include <QtTest/QTest>

#include <functional>
#include <memory>
#include <stdexcept>

using namespace QindaQt::Services;
namespace {
ClipboardModel::ClipboardValue textValue() {
    return {{{QStringLiteral("text/plain"), QByteArray("synthetic privacy fixture")}}};
}
std::unique_ptr<Clipboard::ClipboardHost> makeHost(
    ReentrantWaylandAdapter &adapter, std::function<bool()> admission) {
#ifdef QINDAQT_CLIPBOARD_LEGACY_PRIVACY
    // Old-source reproduction builds the immutable old Host/header, not newer
    // objects or an emulated policy. Its production composition had no guard.
    (void)admission;
    return std::make_unique<Clipboard::ClipboardHost>(&adapter, 44);
#else
    return std::make_unique<Clipboard::ClipboardHost>(&adapter, 44, std::move(admission));
#endif
}
qsizetype entries(const Clipboard::Snapshot &snapshot) {
    return ClipboardModel::decodeDescriptorList(snapshot.descriptorList).descriptors.size();
}
Clipboard::OperationRequest requestFor(
    const Clipboard::Snapshot &snapshot, Clipboard::OperationKind kind) {
    const auto rows = ClipboardModel::decodeDescriptorList(snapshot.descriptorList).descriptors;
    return {kind, 1, snapshot.epoch, snapshot.generation, snapshot.revision,
            rows.isEmpty() ? ClipboardModel::EntryId{} : rows.first().id, false};
}
void enable(Clipboard::ClipboardHost &host, ReentrantWaylandAdapter &adapter) {
    host.setHistoryOptIn(true);
    host.setUnlocked(true);
    adapter.offer(textValue());
}
}

class ClipboardPrivacyAdmissionTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void explicitConsentRemainsIndependent() {
        ReentrantWaylandAdapter adapter;
        auto host = makeHost(adapter, [] { return true; });
        host->setUnlocked(true);
        QVERIFY(host->snapshot().privacyAllowed);
        QVERIFY(!host->snapshot().historyEnabled);
        QVERIFY(!adapter.captureEnabled);
        adapter.offer(textValue());
        QCOMPARE(entries(host->snapshot()), 0);
        host->setHistoryOptIn(true);
        QVERIFY(adapter.captureEnabled);
        adapter.offer(textValue());
        QCOMPARE(entries(host->snapshot()), 1);
        host->setHistoryOptIn(false);
        QCOMPARE(entries(host->snapshot()), 0);
        QVERIFY(!adapter.captureEnabled);
    }
    void emptyAdmissionCannotOpenProductionGate() {
        ReentrantWaylandAdapter adapter;
        auto host = makeHost(adapter, {});
        enable(*host, adapter);
        QVERIFY(!host->snapshot().privacyAllowed);
        QCOMPARE(entries(host->snapshot()), 0);
        QVERIFY(!adapter.captureEnabled);
    }
    void throwingAdmissionIsDenial() {
        ReentrantWaylandAdapter adapter;
        auto host = makeHost(adapter, []() -> bool { throw std::runtime_error("fixture denial"); });
        enable(*host, adapter);
        QVERIFY(!host->snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void synchronousLossPurgesWithoutQueuedSignal() {
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        enable(*host, adapter);
        const auto before = host->snapshot();
        QCOMPARE(entries(before), 1);
        live = false;
        // Deliberately no setUnlocked(false), event-loop turn or prior getter.
        const Clipboard::ClipboardHost &retained = *host;
        const auto denied = retained.snapshot();
        QVERIFY(!denied.privacyAllowed);
        QCOMPARE(denied.generation, before.generation + 1);
        QCOMPARE(entries(denied), 0);
        QVERIFY(!adapter.captureEnabled);
    }
    void revokedReceiptCannotReviveThroughGetter() {
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        enable(*host, adapter);
        live = false;
        QVERIFY(!host->snapshot().privacyAllowed);
        live = true;
        QVERIFY(!host->snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        host->setUnlocked(true); // a fresh authenticated observer event
        QVERIFY(host->snapshot().privacyAllowed);
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void secondSnapshotCheckDropsPreviouslyEncodedCopy() {
        ReentrantWaylandAdapter adapter;
        bool armed = false;
        int calls = 0;
        auto host = makeHost(adapter, [&] { return !armed || ++calls < 2; });
        enable(*host, adapter);
        armed = true;
        const auto snapshot = host->snapshot();
        QVERIFY(!snapshot.privacyAllowed);
        QCOMPARE(entries(snapshot), 0);
        QVERIFY(!adapter.captureEnabled);
    }
    void deniedCaptureCancelsAliasedTransferBeforeReadingIt() {
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        enable(*host, adapter);
        const auto before = host->snapshot();
        live = false;
        adapter.offerAliased(textValue());
        QVERIFY(!adapter.transfer);
        QVERIFY(!adapter.captureEnabled);
        const auto after = host->snapshot();
        QCOMPARE(after.generation, before.generation + 1);
        QCOMPARE(entries(after), 0);
    }
    void captureChangedReceiverCannotRetainNewContent() {
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        host->setHistoryOptIn(true);
        host->setUnlocked(true);
        connect(host.get(), &Clipboard::ClipboardHost::changed, host.get(),
                [&] { live = false; });
        adapter.offerAliased(textValue());
        QVERIFY(!adapter.transfer);
        QVERIFY(!adapter.captureEnabled);
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void copyRechecksAfterAdapterAvailability() {
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        enable(*host, adapter);
        const auto request = requestFor(host->snapshot(), Clipboard::OperationKind::Copy);
        adapter.onAvailable = [&] { live = false; };
        const auto result = host->submit(request);
        QCOMPARE(result.status, Clipboard::OperationStatus::Rejected);
        QCOMPARE(adapter.publications, 0);
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void copyChecksImmediatelyBeforePublish() {
        ReentrantWaylandAdapter adapter;
        bool armed = false;
        int calls = 0;
        auto host = makeHost(adapter, [&] { return !armed || ++calls < 3; });
        enable(*host, adapter);
        const auto request = requestFor(host->snapshot(), Clipboard::OperationKind::Copy);
        armed = true;
        const auto result = host->submit(request);
        QCOMPARE(result.status, Clipboard::OperationStatus::Rejected);
        QCOMPARE(adapter.publications, 0);
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void lossInsidePublishIsUncertain() {
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        enable(*host, adapter);
        const auto request = requestFor(host->snapshot(), Clipboard::OperationKind::Copy);
        adapter.onPublish = [&] { live = false; };
        const auto result = host->submit(request);
        QCOMPARE(adapter.publications, 1);
        QCOMPARE(result.status, Clipboard::OperationStatus::Uncertain);
        QCOMPARE(result.reasonCode, QStringLiteral("privacy-changed"));
        QCOMPARE(entries(host->snapshot()), 0);
        QVERIFY(!adapter.captureEnabled);
    }
    void changedReceiverCannotReturnStaleSuccess_data() {
        QTest::addColumn<Clipboard::OperationKind>("kind");
        QTest::newRow("copy") << Clipboard::OperationKind::Copy;
        QTest::newRow("select") << Clipboard::OperationKind::Select;
        QTest::newRow("delete") << Clipboard::OperationKind::Delete;
        QTest::newRow("clear") << Clipboard::OperationKind::Clear;
    }
    void changedReceiverCannotReturnStaleSuccess() {
        QFETCH(Clipboard::OperationKind, kind);
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        enable(*host, adapter);
        const auto request = requestFor(host->snapshot(), kind);
        connect(host.get(), &Clipboard::ClipboardHost::changed, host.get(),
                [&] { live = false; });
        const auto result = host->submit(request);
        QCOMPARE(result.status, Clipboard::OperationStatus::Uncertain);
        QCOMPARE(result.reasonCode, QStringLiteral("privacy-changed"));
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void reentrantConsentLossCannotLeaveCaptureEnabled() {
        ReentrantWaylandAdapter adapter;
        auto host = makeHost(adapter, [] { return true; });
        host->setHistoryOptIn(true);
        adapter.onCapture = [&](bool enabled) {
            if (enabled) host->setHistoryOptIn(false);
        };
        host->setUnlocked(true);
        QVERIFY(!adapter.captureEnabled);
        QVERIFY(!host->snapshot().historyEnabled);
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void denialRecordsGenerationBeforeReentrantChanged() {
        ReentrantWaylandAdapter adapter;
        bool live = true;
        auto host = makeHost(adapter, [&] { return live; });
        enable(*host, adapter);
        const auto old = host->snapshot();
        bool sawDenied = false;
        connect(host.get(), &Clipboard::ClipboardHost::changed, host.get(), [&] {
            const auto denied = host->snapshot();
            sawDenied = !denied.privacyAllowed && entries(denied) == 0
                && denied.generation == old.generation + 1;
            host->setUnlocked(true); // forbidden reopen during denial publication
        });
        live = false;
        QVERIFY(!host->snapshot().privacyAllowed);
        QVERIFY(sawDenied);
        QCOMPARE(entries(host->snapshot()), 0);
    }
    void reentrantAdmissionCannotDisclose() {
        ReentrantWaylandAdapter adapter;
        bool recurse = false;
        Clipboard::ClipboardHost *borrowed = nullptr;
        auto host = makeHost(adapter, [&] {
            if (recurse) (void)borrowed->snapshot();
            return true;
        });
        borrowed = host.get();
        enable(*host, adapter);
        recurse = true;
        const auto denied = host->snapshot();
        QVERIFY(!denied.privacyAllowed);
        QCOMPARE(entries(denied), 0);
        QVERIFY(!adapter.captureEnabled);
    }
};

QTEST_GUILESS_MAIN(ClipboardPrivacyAdmissionTest)
#include "tst_clipboard_privacy_admission.moc"
