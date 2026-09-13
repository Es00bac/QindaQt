// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_dbus.h>
#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_service/resident_display_service.h>

#include "support/display_service_test_support.h"
#include "support/private_bus_test_support.h"

#include <QtCore/QElapsedTimer>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtTest/QTest>

#include <algorithm>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

class ElapsedClock final : public DisplayTransaction::MonotonicClock
{
public:
    ElapsedClock() { m_elapsed.start(); }

    [[nodiscard]] quint64 nowMilliseconds() const noexcept override
    {
        return static_cast<quint64>(m_elapsed.elapsed());
    }

private:
    QElapsedTimer m_elapsed;
};

class ChangedReceiver final : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void changed(const QString &epoch, const quint64 revision, const bool available)
    {
        Q_UNUSED(epoch)
        revisions.push_back(revision);
        availability.push_back(available);
    }

public:
    QList<quint64> revisions;
    QList<bool> availability;
};

QDBusMessage serviceCall(const QString &method, QVariantList arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(Display::kServiceName), QString::fromLatin1(Display::kObjectPath),
        QString::fromLatin1(Display::kInterfaceName), method);
    message.setArguments(std::move(arguments));
    return message;
}

QDBusArgument onlyArgument(const QDBusPendingCallWatcher &watcher)
{
    const QList<QVariant> arguments = watcher.reply().arguments();
    return arguments.size() == 1 ? qvariant_cast<QDBusArgument>(arguments.constFirst())
                                 : QDBusArgument{};
}

InventoryOutput inventoryOutput(const QString &connector, const QString &uuid,
                                const QRect &geometry, const bool internal)
{
    InventoryOutput value = output(connector, geometry);
    value.runtimeCompositorUuid = uuid;
    value.internal = internal;
    return value;
}

DeviceBrightnessFrame devices(const quint32 externalValue)
{
    const auto device = [](const char *connector, const char *uuid, const quint32 value) {
        return DeviceBrightness{.connectorName = QString::fromLatin1(connector),
                                .runtimeUuid = QString::fromLatin1(uuid),
                                .enabled = true,
                                .capable = true,
                                .observed = true,
                                .value = value};
    };
    return {.ownerGeneration = 5,
            .devices = {device("DP-1", "uuid-dp", externalValue),
                        device("eDP-1", "uuid-edp", 3'000)}};
}

// One private bus, resident, and client. Every value crosses a real session
// bus message; the port and inventory are the only fakes.
class Resident
{
public:
    explicit Resident(const quint64 observationTimeoutMilliseconds = 2'000)
    {
        QString error;
        started = bus.start(&error);
        if (!started) {
            qWarning("%s", qPrintable(error));
            return;
        }
        Display::registerDBusTypes();
        residentName = privateConnectionName(QStringLiteral("brightness-resident"));
        clientName = privateConnectionName(QStringLiteral("brightness-client"));
        QDBusConnection residentConnection =
            QDBusConnection::connectToBus(bus.address(), residentName);
        client = std::make_unique<QDBusConnection>(
            QDBusConnection::connectToBus(bus.address(), clientName));
        auto inventoryOwned = std::make_unique<FakeInventorySource>();
        inventory = inventoryOwned.get();
        auto portOwned = std::make_unique<FakeTransactionPort>();
        port = portOwned.get();
        const DisplayTransaction::Timing timing{
            .applyTimeoutMilliseconds = 5'000,
            .observationTimeoutMilliseconds = observationTimeoutMilliseconds,
            .confirmationTimeoutMilliseconds = 60'000,
            .firstRevertBackoffMilliseconds = 20,
            .secondRevertBackoffMilliseconds = 20};
        service = std::make_unique<ResidentDisplayService>(
            std::move(inventoryOwned), std::move(portOwned), std::make_unique<ElapsedClock>(),
            [] { return QStringLiteral("brightness-restart-seed"); }, residentConnection,
            QString::fromLatin1(Display::kServiceName), timing);
        started = residentConnection.isConnected() && client->isConnected()
            && service->start() == ServiceStartStatus::Started
            && client->connect(QString::fromLatin1(Display::kServiceName),
                               QString::fromLatin1(Display::kObjectPath),
                               QString::fromLatin1(Display::kInterfaceName),
                               QStringLiteral("Changed"), &changed,
                               SLOT(changed(QString,quint64,bool)));
    }

    ~Resident()
    {
        service.reset();
        client.reset();
        QDBusConnection::disconnectFromBus(clientName);
        QDBusConnection::disconnectFromBus(residentName);
    }

    [[nodiscard]] std::unique_ptr<QDBusPendingCallWatcher> call(const QString &method,
                                                                QVariantList arguments = {})
    {
        return std::make_unique<QDBusPendingCallWatcher>(
            client->asyncCall(serviceCall(method, std::move(arguments)), 5'000));
    }

    [[nodiscard]] QString stableId(const QString &connector) const
    {
        const Display::Snapshot *snapshot = service->model()->snapshot();
        const auto found =
            std::ranges::find(snapshot->outputs, connector, &Display::Output::connectorName);
        return found == snapshot->outputs.cend() ? QString{} : found->stableId;
    }

    [[nodiscard]] bool publishTopologyAndDevices()
    {
        inventory->publish(frame(
            1, {inventoryOutput(QStringLiteral("DP-1"), QStringLiteral("uuid-dp"),
                                QRect(0, 0, 1920, 1080), false),
                inventoryOutput(QStringLiteral("eDP-1"), QStringLiteral("uuid-edp"),
                                QRect(1920, 0, 1920, 1080), true)}));
        (void)service->setSafetyState(DisplayTransaction::SafetyState::Safe);
        port->publishDevices(devices(6'000));
        const Display::BrightnessSnapshot *published = service->model()->brightnessSnapshot();
        const bool joined = published != nullptr
            && std::ranges::any_of(published->outputs, &Display::OutputBrightness::capable);
        // The topology, Safe, and device republish each emit a hint. Drain
        // them before a test counts the hint of a later republish.
        QTest::qWait(200);
        return joined && !changed.revisions.isEmpty();
    }

    PrivateSessionBus bus;
    QString residentName;
    QString clientName;
    std::unique_ptr<QDBusConnection> client;
    FakeInventorySource *inventory = nullptr;
    FakeTransactionPort *port = nullptr;
    std::unique_ptr<ResidentDisplayService> service;
    ChangedReceiver changed;
    bool started = false;
};

bool finished(const std::unique_ptr<QDBusPendingCallWatcher> &watcher)
{
    return watcher->isFinished();
}

Display::OperationResult decodedResult(const QDBusPendingCallWatcher &watcher, bool *accepted)
{
    Display::OperationResult result;
    *accepted = !watcher.isError()
        && Display::decodeOperationResultArgument(onlyArgument(watcher), result).accepted;
    return result;
}

} // namespace

class ResidentDisplayBrightnessPrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesJoinedBrightnessAndRepliesAfterObservedRepublish();
    void refusesHostileAndInadmissibleRequestsOverTheBus();
    void timesOutUnobservedWriteAndDeliversStopUncertainty();
};

void ResidentDisplayBrightnessPrivateBusTest::publishesJoinedBrightnessAndRepliesAfterObservedRepublish()
{
    Resident r;
    QVERIFY(r.started);
    const auto unavailable = r.call(QStringLiteral("GetBrightness"));
    QTRY_VERIFY_WITH_TIMEOUT(finished(unavailable), 5'000);
    QVERIFY(unavailable->isError());
    QCOMPARE(unavailable->error().name(), QStringLiteral("org.qindaqt.Display1.Error.Unavailable"));

    QVERIFY(r.publishTopologyAndDevices());
    // Device facts republish brightness without advancing the topology revision.
    QCOMPARE(r.changed.revisions.constLast(), quint64{1});

    const auto snapshotCall = r.call(QStringLiteral("GetSnapshot"));
    const auto brightnessCall = r.call(QStringLiteral("GetBrightness"));
    QTRY_VERIFY_WITH_TIMEOUT(finished(snapshotCall) && finished(brightnessCall), 5'000);
    Display::Snapshot snapshot;
    QVERIFY(Display::decodeSnapshotArgument(onlyArgument(*snapshotCall), snapshot).accepted);
    Display::BrightnessSnapshot brightness;
    const Display::DBusDecodeResult decoded =
        Display::decodeBrightnessSnapshotArgument(onlyArgument(*brightnessCall), brightness);
    QVERIFY2(decoded.accepted, qPrintable(decoded.reasonCode));
    QVERIFY(Display::validateBrightnessJoin(snapshot, brightness).accepted);
    QCOMPARE(brightness.revision, quint64{2});
    const QString externalId = r.stableId(QStringLiteral("DP-1"));
    const auto external = std::ranges::find(brightness.outputs, externalId,
                                            &Display::OutputBrightness::stableId);
    QVERIFY(external != brightness.outputs.cend());
    QCOMPARE(*external, (Display::OutputBrightness{.stableId = externalId,
                                                   .capable = true,
                                                   .observed = true,
                                                   .value = 6'000}));

    const Display::BrightnessRequest request{.baseEpoch = brightness.serviceEpoch,
                                             .baseRevision = brightness.revision,
                                             .stableId = externalId,
                                             .value = 2'500};
    const auto setCall =
        r.call(QStringLiteral("SetOutputBrightness"), {QVariant::fromValue(request)});
    // Record the hint count when the client delivers the reply. Delivery
    // follows bus message order, so a reply sent before its proving hint
    // is caught here, not masked by a later event-loop pass.
    qsizetype changedAtReply = -1;
    QObject::connect(setCall.get(), &QDBusPendingCallWatcher::finished, setCall.get(),
                     [&changedAtReply, &r] { changedAtReply = r.changed.revisions.size(); });
    QTRY_COMPARE_WITH_TIMEOUT(r.port->brightnessRequests.size(), 1, 5'000);
    QCOMPARE(r.port->brightnessRequests.constFirst().connectorName, QStringLiteral("DP-1"));
    QCOMPARE(r.port->brightnessRequests.constFirst().value, quint32{2'500});
    QTest::qWait(50);
    QVERIFY2(!setCall->isFinished(), "an accepted immediate request replies only when finished");

    r.port->completeBrightness(BrightnessApplyOutcome::Applied);
    QTest::qWait(50);
    QVERIFY2(!setCall->isFinished(), "a compositor acknowledgement alone is not Applied");

    const qsizetype changedBeforeObservation = r.changed.revisions.size();
    r.port->publishDevices(devices(2'500));
    QTRY_VERIFY_WITH_TIMEOUT(changedAtReply >= 0, 5'000);
    QVERIFY2(changedAtReply > changedBeforeObservation,
             "the Changed hint for the proving republish precedes the reply");
    bool accepted = false;
    const Display::OperationResult result = decodedResult(*setCall, &accepted);
    QVERIFY(accepted);
    QCOMPARE(result.kind, Display::OperationKind::ImmediatePolicy);
    QCOMPARE(result.status, Display::OperationStatus::Succeeded);
    QCOMPARE(result.initiatingRevision, quint64{2});
    QCOMPARE(result.observedRevision, quint64{3});

    const auto after = r.call(QStringLiteral("GetBrightness"));
    QTRY_VERIFY_WITH_TIMEOUT(finished(after), 5'000);
    QVERIFY(Display::decodeBrightnessSnapshotArgument(onlyArgument(*after), brightness).accepted);
    QCOMPARE(brightness.revision, result.observedRevision);
    QCOMPARE(std::ranges::find(brightness.outputs, externalId, &Display::OutputBrightness::stableId)
                 ->value,
             quint32{2'500});
}

void ResidentDisplayBrightnessPrivateBusTest::refusesHostileAndInadmissibleRequestsOverTheBus()
{
    Resident r;
    QVERIFY(r.started);
    QVERIFY(r.publishTopologyAndDevices());
    const Display::BrightnessSnapshot *published = r.service->model()->brightnessSnapshot();
    QVERIFY(published != nullptr);

    struct Case {
        QString stableId;
        quint32 value;
        Display::ErrorCode error;
        const char *diagnostic;
    };
    const QList<Case> cases{
        {r.stableId(QStringLiteral("DP-1")), Display::kMaxBrightness + 1,
         Display::ErrorCode::InvalidCandidate, "invalid-brightness-value"},
        {r.stableId(QStringLiteral("eDP-1")), 1'000, Display::ErrorCode::InvalidCandidate,
         "internal-output"},
        {QStringLiteral("conn:DP-9"), 1'000, Display::ErrorCode::InvalidCandidate,
         "unknown-output"},
    };
    for (const Case &c : cases) {
        const Display::BrightnessRequest request{.baseEpoch = published->serviceEpoch,
                                                 .baseRevision = published->revision,
                                                 .stableId = c.stableId,
                                                 .value = c.value};
        const auto call =
            r.call(QStringLiteral("SetOutputBrightness"), {QVariant::fromValue(request)});
        QTRY_VERIFY_WITH_TIMEOUT(finished(call), 5'000);
        bool accepted = false;
        const Display::OperationResult result = decodedResult(*call, &accepted);
        QVERIFY2(accepted, c.diagnostic);
        QCOMPARE(result.status, Display::OperationStatus::Rejected);
        QCOMPARE(result.error, c.error);
        QCOMPARE(result.diagnostic, QString::fromLatin1(c.diagnostic));
    }

    // A type-divergent argument never reaches the model.
    const auto divergent =
        r.call(QStringLiteral("SetOutputBrightness"), {QStringLiteral("not-a-request")});
    QTRY_VERIFY_WITH_TIMEOUT(finished(divergent), 5'000);
    QVERIFY(divergent->isError());
    QVERIFY(r.port->brightnessRequests.isEmpty());
}

void ResidentDisplayBrightnessPrivateBusTest::timesOutUnobservedWriteAndDeliversStopUncertainty()
{
    Resident r(150);
    QVERIFY(r.started);
    QVERIFY(r.publishTopologyAndDevices());
    const QString externalId = r.stableId(QStringLiteral("DP-1"));
    const auto requestAt = [&r, &externalId](const quint32 value) {
        const Display::BrightnessSnapshot *published = r.service->model()->brightnessSnapshot();
        return QVariant::fromValue(Display::BrightnessRequest{.baseEpoch = published->serviceEpoch,
                                                              .baseRevision = published->revision,
                                                              .stableId = externalId,
                                                              .value = value});
    };

    // KWin 6.6.6 acknowledges set_brightness on outputs that ignore it; without
    // an observed republish the result is typed uncertainty, never Applied.
    const auto ignored = r.call(QStringLiteral("SetOutputBrightness"), {requestAt(2'500)});
    QTRY_COMPARE_WITH_TIMEOUT(r.port->brightnessRequests.size(), 1, 5'000);
    r.port->completeBrightness(BrightnessApplyOutcome::Applied);
    QTRY_VERIFY_WITH_TIMEOUT(finished(ignored), 5'000);
    bool accepted = false;
    Display::OperationResult result = decodedResult(*ignored, &accepted);
    QVERIFY(accepted);
    QCOMPARE(result.status, Display::OperationStatus::Uncertain);
    QCOMPARE(result.error, Display::ErrorCode::Timeout);
    QCOMPARE(result.diagnostic, QStringLiteral("brightness-observation-timeout"));

    const auto stopped = r.call(QStringLiteral("SetOutputBrightness"), {requestAt(2'600)});
    QTRY_COMPARE_WITH_TIMEOUT(r.port->brightnessRequests.size(), 2, 5'000);
    r.service->stop();
    QTRY_VERIFY_WITH_TIMEOUT(finished(stopped), 5'000);
    result = decodedResult(*stopped, &accepted);
    QVERIFY(accepted);
    QCOMPARE(result.status, Display::OperationStatus::Uncertain);
    QCOMPARE(result.error, Display::ErrorCode::CompositorUnavailable);
    QCOMPARE(result.diagnostic, QStringLiteral("service-lineage-lost"));
    QCOMPARE(r.port->brightnessRequests.size(), 2);
}

QTEST_GUILESS_MAIN(ResidentDisplayBrightnessPrivateBusTest)
#include "tst_resident_display_brightness_private_bus.moc"
