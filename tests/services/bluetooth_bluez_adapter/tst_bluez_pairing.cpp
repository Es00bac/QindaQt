// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/bluez_harness.h"

#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest>

using namespace QindaQt::Bluetooth;
using BluezHarness = QindaQt::Tests::BluezHarness;
using FakeBluez = QindaQt::Tests::FakeBluez;

class BluezPairingTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void interactivePromptRoundTrip_data();
    void interactivePromptRoundTrip();
    void rejectedConfirmationRejectsPair();
    void displayPromptCanBeCanceled_data();
    void displayPromptCanBeCanceled();
    void promptTimeoutFailsClosed();
    void stalePromptReplyCannotAuthorizeReplacement();
    void unregistersAgentOnAdapterLossAndShutdown();
    void cancelPairingUsesCanonicalReason();
    void ownerLossFailsClosed();
    void foreignCallerCannotCreatePrompt();
    void malformedDisplayFailsClosed_data();
    void malformedDisplayFailsClosed();
    void trustedAndRemoveRoundTrip();

private:
    static constexpr auto kAdapterAddress = "AA:BB:CC:00:11:22";
    static constexpr auto kDeviceAddress = "AA:BB:CC:33:44:55";

    static OperationRequest replyFor(const PairingPrompt &prompt,
                                     const OperationKind kind,
                                     const QString &input = {})
    {
        OperationRequest request{.kind = kind,
                                 .target = prompt.device,
                                 .promptId = prompt.promptId};
        if (!input.isEmpty()) {
            const bool encoded = setPairingInput(request.input, request.inputSize, input);
            Q_ASSERT(encoded);
        }
        return request;
    }
};

void BluezPairingTests::interactivePromptRoundTrip_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<quint32>("promptKind");
    QTest::addColumn<quint32>("replyKind");
    QTest::addColumn<QString>("input");

    QTest::newRow("confirm-passkey")
        << int(FakeBluez::PairingMode::ConfirmPasskey)
        << quint32(PairingPromptKind::ConfirmPasskey)
        << quint32(OperationKind::ReplyConfirmation) << QString{};
    QTest::newRow("enter-passkey")
        << int(FakeBluez::PairingMode::RequestPasskey)
        << quint32(PairingPromptKind::EnterPasskey)
        << quint32(OperationKind::ReplyPasskey) << QStringLiteral("654321");
    QTest::newRow("enter-pin")
        << int(FakeBluez::PairingMode::RequestPin)
        << quint32(PairingPromptKind::EnterPin)
        << quint32(OperationKind::ReplyPin) << QStringLiteral("A1b2");
    QTest::newRow("authorize-service")
        << int(FakeBluez::PairingMode::AuthorizeService)
        << quint32(PairingPromptKind::AuthorizeService)
        << quint32(OperationKind::ReplyConfirmation) << QString{};
}

void BluezPairingTests::interactivePromptRoundTrip()
{
    QFETCH(int, mode);
    QFETCH(quint32, promptKind);
    QFETCH(quint32, replyKind);
    QFETCH(QString, input);

    BluezHarness harness(7201);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress),
        QStringLiteral("Internal"), true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QString::fromLatin1(kDeviceAddress), QStringLiteral("Keyboard"));
    harness.fake->device(devicePath)->pairingMode =
        static_cast<FakeBluez::PairingMode>(mode);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] {
        return harness.fake->registerAgentCalls == 1;
    }));
    QCOMPARE(harness.fake->registeredCapability, QStringLiteral("KeyboardDisplay"));
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);

    const Handle device = harness.model->snapshot().devices.constFirst().handle;
    const OperationSubmission pairing = harness.model->submit(
        {.kind = OperationKind::Pair, .target = device}, QStringLiteral(":1.20"));
    QVERIFY(pairing.pending);
    QVERIFY(harness.waitUntil([&harness, promptKind] {
        return quint32(harness.model->snapshot().pairingPrompt.kind) == promptKind;
    }));
    const PairingPrompt prompt = harness.model->snapshot().pairingPrompt;
    OperationRequest reply = replyFor(
        prompt, static_cast<OperationKind>(replyKind), input);
    reply.accepted = true;
    const OperationSubmission answered =
        harness.model->submit(reply, QStringLiteral(":1.20"));
    QVERIFY(answered.pending);
    QCOMPARE(harness.awaitResult(completed, answered.operationId)->status,
             OperationStatus::Succeeded);
    const std::optional<OperationResult> pairResult =
        harness.awaitResult(completed, pairing.operationId);
    QVERIFY(pairResult.has_value());
    QCOMPARE(pairResult->status, OperationStatus::Succeeded);
    QCOMPARE(pairResult->reasonCode, QStringLiteral("paired"));
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().devices.constFirst().paired
            && !harness.model->snapshot().pairingPrompt.active();
    }));
    if (static_cast<OperationKind>(replyKind) == OperationKind::ReplyPasskey) {
        QCOMPARE(harness.fake->lastPasskeyReply, quint32(654321));
    } else if (static_cast<OperationKind>(replyKind) == OperationKind::ReplyPin) {
        QCOMPARE(harness.fake->lastPinReply, QStringLiteral("A1b2"));
    }
}

void BluezPairingTests::rejectedConfirmationRejectsPair()
{
    BluezHarness harness(7202);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    (void)harness.fake->addDevice(adapterPath, QString::fromLatin1(kDeviceAddress), {});
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const OperationSubmission pairing = harness.model->submit(
        {.kind = OperationKind::Pair,
         .target = harness.model->snapshot().devices.constFirst().handle},
        QStringLiteral(":1.21"));
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().pairingPrompt.active();
    }));
    OperationRequest reply = replyFor(harness.model->snapshot().pairingPrompt,
                                      OperationKind::ReplyConfirmation);
    reply.accepted = false;
    const OperationSubmission answered =
        harness.model->submit(reply, QStringLiteral(":1.21"));
    QCOMPARE(harness.awaitResult(completed, answered.operationId)->status,
             OperationStatus::Succeeded);
    QCOMPARE(harness.awaitResult(completed, pairing.operationId)->status,
             OperationStatus::Rejected);
    QVERIFY(!harness.model->snapshot().devices.constFirst().paired);
}

void BluezPairingTests::displayPromptCanBeCanceled_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<quint32>("promptKind");
    QTest::newRow("display-passkey") << int(FakeBluez::PairingMode::DisplayPasskey)
                                     << quint32(PairingPromptKind::DisplayPasskey);
    QTest::newRow("display-pin") << int(FakeBluez::PairingMode::DisplayPin)
                                 << quint32(PairingPromptKind::DisplayPin);
}

void BluezPairingTests::displayPromptCanBeCanceled()
{
    QFETCH(int, mode);
    QFETCH(quint32, promptKind);
    BluezHarness harness(7203);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QString::fromLatin1(kDeviceAddress), {});
    harness.fake->device(devicePath)->pairingMode =
        static_cast<FakeBluez::PairingMode>(mode);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const OperationSubmission pairing = harness.model->submit(
        {.kind = OperationKind::Pair,
         .target = harness.model->snapshot().devices.constFirst().handle},
        QStringLiteral(":1.22"));
    QVERIFY(harness.waitUntil([&harness, promptKind] {
        return quint32(harness.model->snapshot().pairingPrompt.kind) == promptKind;
    }));
    const OperationSubmission canceled = harness.model->submit(
        replyFor(harness.model->snapshot().pairingPrompt, OperationKind::CancelPrompt),
        QStringLiteral(":1.22"));
    const std::optional<OperationResult> cancelResult =
        harness.awaitResult(completed, canceled.operationId);
    QVERIFY(cancelResult.has_value());
    QCOMPARE(cancelResult->status, OperationStatus::Succeeded);
    QCOMPARE(cancelResult->reasonCode, QStringLiteral("prompt-cancelled"));
    QCOMPARE(harness.awaitResult(completed, pairing.operationId)->status,
             OperationStatus::Rejected);
    QCOMPARE(harness.fake->cancelPairingCalls, 1);
    QVERIFY(!harness.model->snapshot().pairingPrompt.active());
}

void BluezPairingTests::stalePromptReplyCannotAuthorizeReplacement()
{
    BluezHarness harness(7209);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    (void)harness.fake->addDevice(adapterPath, QString::fromLatin1(kDeviceAddress), {});
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle device = harness.model->snapshot().devices.constFirst().handle;

    const OperationSubmission firstPair = harness.model->submit(
        {.kind = OperationKind::Pair, .target = device}, QStringLiteral(":1.27"));
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().pairingPrompt.active();
    }));
    const PairingPrompt stalePrompt = harness.model->snapshot().pairingPrompt;
    const OperationSubmission firstCancel = harness.model->submit(
        {.kind = OperationKind::CancelPairing, .target = device},
        QStringLiteral(":1.27"));
    QCOMPARE(harness.awaitResult(completed, firstCancel.operationId)->status,
             OperationStatus::Succeeded);
    QCOMPARE(harness.awaitResult(completed, firstPair.operationId)->status,
             OperationStatus::Rejected);

    const OperationSubmission secondPair = harness.model->submit(
        {.kind = OperationKind::Pair, .target = device}, QStringLiteral(":1.27"));
    QVERIFY(harness.waitUntil([&harness, stalePrompt] {
        const PairingPrompt current = harness.model->snapshot().pairingPrompt;
        return current.active() && current.promptId != stalePrompt.promptId;
    }));
    OperationRequest staleReply = replyFor(stalePrompt,
                                           OperationKind::ReplyConfirmation);
    staleReply.accepted = true;
    const OperationSubmission rejected = harness.model->submit(
        staleReply, QStringLiteral(":1.27"));
    QVERIFY(!rejected.pending);
    QCOMPARE(rejected.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(rejected.immediateResult.reasonCode, QStringLiteral("no-prompt"));
    QVERIFY(harness.model->snapshot().pairingPrompt.active());
    QVERIFY(!harness.model->snapshot().devices.constFirst().paired);

    OperationRequest cleanup = replyFor(harness.model->snapshot().pairingPrompt,
                                        OperationKind::ReplyConfirmation);
    cleanup.accepted = false;
    const OperationSubmission cleanupReply = harness.model->submit(
        cleanup, QStringLiteral(":1.27"));
    QCOMPARE(harness.awaitResult(completed, cleanupReply.operationId)->status,
             OperationStatus::Succeeded);
    QCOMPARE(harness.awaitResult(completed, secondPair.operationId)->status,
             OperationStatus::Rejected);
}

void BluezPairingTests::unregistersAgentOnAdapterLossAndShutdown()
{
    BluezHarness harness(7210);
    QVERIFY(harness.ready());
    const QString firstAdapter = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));

    harness.fake->removeAdapterObject(firstAdapter);
    QVERIFY(harness.waitUntil([&harness] {
        return harness.fake->unregisterAgentCalls == 1;
    }));
    (void)harness.fake->addAdapter(
        QStringLiteral("hci1"), QStringLiteral("AA:BB:CC:00:11:23"), {}, true);
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 2; }));

    harness.model->stop();
    QVERIFY(harness.waitUntil([&harness] {
        return harness.fake->unregisterAgentCalls == 2;
    }));
}

void BluezPairingTests::cancelPairingUsesCanonicalReason()
{
    BluezHarness harness(7211);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    (void)harness.fake->addDevice(adapterPath, QString::fromLatin1(kDeviceAddress), {});
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle device = harness.model->snapshot().devices.constFirst().handle;
    const OperationSubmission pairing = harness.model->submit(
        {.kind = OperationKind::Pair, .target = device}, QStringLiteral(":1.28"));
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().pairingPrompt.active();
    }));
    const OperationSubmission canceled = harness.model->submit(
        {.kind = OperationKind::CancelPairing, .target = device},
        QStringLiteral(":1.28"));
    const std::optional<OperationResult> result =
        harness.awaitResult(completed, canceled.operationId);
    QVERIFY(result.has_value());
    QCOMPARE(result->status, OperationStatus::Succeeded);
    QCOMPARE(result->reasonCode, QStringLiteral("pairing-cancelled"));
    QCOMPARE(harness.awaitResult(completed, pairing.operationId)->status,
             OperationStatus::Rejected);
}

void BluezPairingTests::promptTimeoutFailsClosed()
{
    BluezHarness harness(7204, 20);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    (void)harness.fake->addDevice(adapterPath, QString::fromLatin1(kDeviceAddress), {});
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const OperationSubmission pairing = harness.model->submit(
        {.kind = OperationKind::Pair,
         .target = harness.model->snapshot().devices.constFirst().handle},
        QStringLiteral(":1.23"));
    const std::optional<OperationResult> result =
        harness.awaitResult(completed, pairing.operationId);
    QVERIFY(result.has_value());
    QCOMPARE(result->status, OperationStatus::Rejected);
    QCOMPARE(result->reasonCode, QStringLiteral("pairing-rejected"));
    QVERIFY(!harness.model->snapshot().pairingPrompt.active());
}

void BluezPairingTests::ownerLossFailsClosed()
{
    BluezHarness harness(7205);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    (void)harness.fake->addDevice(adapterPath, QString::fromLatin1(kDeviceAddress), {});
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const OperationSubmission pairing = harness.model->submit(
        {.kind = OperationKind::Pair,
         .target = harness.model->snapshot().devices.constFirst().handle},
        QStringLiteral(":1.24"));
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().pairingPrompt.active();
    }));
    harness.fake->dropOwnership();
    QVERIFY(harness.waitUnavailable());
    const std::optional<OperationResult> result =
        harness.awaitResult(completed, pairing.operationId);
    QVERIFY(result.has_value());
    QCOMPARE(result->status, OperationStatus::Uncertain);
    QVERIFY(!harness.model->snapshot().pairingPrompt.active());
}

void BluezPairingTests::foreignCallerCannotCreatePrompt()
{
    BluezHarness harness(7206);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QString::fromLatin1(kDeviceAddress), {});
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));

    QDBusMessage call = QDBusMessage::createMethodCall(
        harness.client.baseService(), QStringLiteral("/org/qindaqt/BluetoothAgent"),
        QStringLiteral("org.bluez.Agent1"), QStringLiteral("RequestConfirmation"));
    call.setArguments({QVariant::fromValue(QDBusObjectPath(devicePath)), quint32(123456)});
    QDBusPendingCallWatcher watcher(harness.bus.connection.asyncCall(call));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE(finished.size(), 1);
    const QDBusPendingReply<> reply = watcher;
    QVERIFY(reply.isError());
    QCOMPARE(reply.error().name(), QStringLiteral("org.bluez.Error.Rejected"));
    QVERIFY(!harness.model->snapshot().pairingPrompt.active());
    QCOMPARE(harness.model->snapshot().availability, Availability::Ready);
}

void BluezPairingTests::malformedDisplayFailsClosed_data()
{
    QTest::addColumn<quint32>("passkey");
    QTest::addColumn<quint16>("entered");
    QTest::newRow("passkey-overflow") << quint32(1'000'000) << quint16(0);
    QTest::newRow("entered-overflow") << quint32(123456) << quint16(7);
}

void BluezPairingTests::malformedDisplayFailsClosed()
{
    QFETCH(quint32, passkey);
    QFETCH(quint16, entered);
    BluezHarness harness(7207);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QString::fromLatin1(kDeviceAddress), {});
    FakeBluez::DeviceEntity *device = harness.fake->device(devicePath);
    device->pairingMode = FakeBluez::PairingMode::DisplayPasskey;
    device->pairingPasskey = passkey;
    device->entered = entered;
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QVERIFY(harness.waitUntil([&harness] { return harness.fake->registerAgentCalls == 1; }));
    const OperationSubmission pairing = harness.model->submit(
        {.kind = OperationKind::Pair,
         .target = harness.model->snapshot().devices.constFirst().handle},
        QStringLiteral(":1.25"));
    QVERIFY(pairing.pending);
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().availability == Availability::Degraded;
    }));
    QCOMPARE(harness.model->snapshot().reasonCode, QStringLiteral("backend-malformed"));
    QVERIFY(!harness.model->snapshot().pairingPrompt.active());
}

void BluezPairingTests::trustedAndRemoveRoundTrip()
{
    BluezHarness harness(7208);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), QString::fromLatin1(kAdapterAddress), {}, true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QString::fromLatin1(kDeviceAddress), {});
    harness.fake->device(devicePath)->paired = true;
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle handle = harness.model->snapshot().devices.constFirst().handle;
    const OperationSubmission trusted = harness.model->submit(
        {.kind = OperationKind::SetTrusted, .target = handle, .trusted = true},
        QStringLiteral(":1.26"));
    QCOMPARE(harness.awaitResult(completed, trusted.operationId)->status,
             OperationStatus::Succeeded);
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().devices.constFirst().trusted;
    }));
    const OperationSubmission removed = harness.model->submit(
        {.kind = OperationKind::RemoveDevice, .target = handle},
        QStringLiteral(":1.26"));
    QCOMPARE(harness.awaitResult(completed, removed.operationId)->status,
             OperationStatus::Succeeded);
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().devices.isEmpty();
    }));
    QCOMPARE(harness.fake->removeDeviceCalls, 1);
}

QTEST_GUILESS_MAIN(BluezPairingTests)

#include "tst_bluez_pairing.moc"
