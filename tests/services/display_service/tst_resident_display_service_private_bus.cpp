// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_dbus.h>
#include <qindaqt/services/display_service/resident_display_service.h>
#include <qindaqt/services/display_topology/topology.h>

#include "support/display_service_test_support.h"
#include "support/private_bus_test_support.h"

#include <QtCore/QElapsedTimer>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest/QTest>

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
        epochs.push_back(epoch);
        revisions.push_back(revision);
        availability.push_back(available);
    }

public:
    QStringList epochs;
    QList<quint64> revisions;
    QList<bool> availability;
};

QDBusMessage serviceCall(const QString &destination, const QString &method,
                         QVariantList arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        destination, QString::fromLatin1(Display::kObjectPath),
        QString::fromLatin1(Display::kInterfaceName), method);
    message.setArguments(std::move(arguments));
    return message;
}

} // namespace

class ResidentDisplayServicePrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void registersPublishesTicksAndTearsDown();
};

void ResidentDisplayServicePrivateBusTest::registersPublishesTicksAndTearsDown()
{
    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));
    Display::registerDBusTypes();

    const QString residentName = privateConnectionName(QStringLiteral("resident"));
    const QString clientName = privateConnectionName(QStringLiteral("resident-client"));
    QDBusConnection residentConnection =
        QDBusConnection::connectToBus(bus.address(), residentName);
    QDBusConnection client = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(residentConnection.isConnected());
    QVERIFY(client.isConnected());
    const QString residentUniqueOwner = residentConnection.baseService();

    auto inventory = std::make_unique<FakeInventorySource>();
    FakeInventorySource *inventoryPointer = inventory.get();
    auto port = std::make_unique<FakeTransactionPort>();
    FakeTransactionPort *portPointer = port.get();
    auto clock = std::make_unique<ElapsedClock>();
    const DisplayTransaction::Timing timing{
        .applyTimeoutMilliseconds = 40,
        .observationTimeoutMilliseconds = 40,
        .confirmationTimeoutMilliseconds = 80,
        .firstRevertBackoffMilliseconds = 20,
        .secondRevertBackoffMilliseconds = 20};
    ResidentDisplayService service(
        std::move(inventory), std::move(port), std::move(clock),
        [] { return QStringLiteral("private-restart-seed"); }, residentConnection,
        QString::fromLatin1(Display::kServiceName), timing);
    QCOMPARE(service.start(), ServiceStartStatus::Started);
    QVERIFY(service.isRunning());
    QVERIFY(inventoryPointer->started);
    QTRY_VERIFY_WITH_TIMEOUT(
        client.interface()->isServiceRegistered(
                              QString::fromLatin1(Display::kServiceName))
            .value(),
        5'000);

    ChangedReceiver changed;
    QVERIFY(client.connect(QString::fromLatin1(Display::kServiceName),
                           QString::fromLatin1(Display::kObjectPath),
                           QString::fromLatin1(Display::kInterfaceName),
                           QStringLiteral("Changed"), &changed,
                           SLOT(changed(QString,quint64,bool))));

    QDBusPendingCallWatcher unavailableWatcher(client.asyncCall(
        serviceCall(QString::fromLatin1(Display::kServiceName),
                    QStringLiteral("GetSnapshot")),
        5'000));
    QTRY_VERIFY_WITH_TIMEOUT(unavailableWatcher.isFinished(), 5'000);
    const QDBusPendingReply<Display::Snapshot> unavailableReply(unavailableWatcher);
    QVERIFY(unavailableReply.isError());
    QCOMPARE(unavailableReply.error().name(),
             QStringLiteral("org.qindaqt.Display1.Error.Unavailable"));

    inventoryPointer->publish(frame(1, {output()}));
    QTRY_COMPARE_WITH_TIMEOUT(changed.epochs.size(), 1, 5'000);
    QVERIFY(changed.availability.constLast());
    QVERIFY(!changed.epochs.constLast().isEmpty());
    QCOMPARE(changed.revisions.constLast(), quint64(1));

    QDBusPendingCallWatcher snapshotWatcher(client.asyncCall(
        serviceCall(QString::fromLatin1(Display::kServiceName),
                    QStringLiteral("GetSnapshot")),
        5'000));
    QTRY_VERIFY_WITH_TIMEOUT(snapshotWatcher.isFinished(), 5'000);
    const QDBusPendingReply<Display::Snapshot> snapshotReply(snapshotWatcher);
    QVERIFY2(snapshotReply.isValid(), qPrintable(snapshotReply.error().message()));
    QCOMPARE(snapshotReply.value().serviceEpoch, changed.epochs.constLast());
    QCOMPARE(snapshotReply.value().revision, quint64(1));

    InventoryOutput changedOutput = output();
    changedOutput.model = QStringLiteral("Changed model");
    inventoryPointer->publish(frame(2, {changedOutput}));
    QTRY_COMPARE_WITH_TIMEOUT(changed.epochs.size(), 2, 5'000);
    QCOMPARE(changed.revisions.constLast(), quint64(2));
    QCOMPARE(changed.epochs.constLast(), snapshotReply.value().serviceEpoch);

    QVERIFY(service.model()->safetyChanged(
                DisplayTransaction::SafetyState::Safe)
                .accepted);
    Display::Candidate candidate =
        DisplayTopology::candidateFromSnapshot(*service.model()->snapshot());
    candidate.outputs[0].transform = Display::Transform::Rotate180;
    QDBusPendingCallWatcher stageWatcher(client.asyncCall(
        serviceCall(QString::fromLatin1(Display::kServiceName),
                    QStringLiteral("Stage"),
                    {QStringLiteral("private-timer"),
                     QVariant::fromValue(candidate)}),
        5'000));
    QTRY_VERIFY_WITH_TIMEOUT(stageWatcher.isFinished(), 5'000);
    const QDBusPendingReply<Display::OperationResult> stageReply(stageWatcher);
    QVERIFY2(stageReply.isValid(), qPrintable(stageReply.error().message()));
    QCOMPARE(stageReply.value().status, Display::OperationStatus::Accepted);

    QDBusPendingCallWatcher previewWatcher(client.asyncCall(
        serviceCall(QString::fromLatin1(Display::kServiceName),
                    QStringLiteral("Preview"),
                    {QStringLiteral("private-timer")}),
        5'000));
    QTRY_VERIFY_WITH_TIMEOUT(previewWatcher.isFinished(), 5'000);
    const QDBusPendingReply<Display::OperationResult> previewReply(previewWatcher);
    QVERIFY2(previewReply.isValid(), qPrintable(previewReply.error().message()));
    QCOMPARE(previewReply.value().status, Display::OperationStatus::Accepted);
    QCOMPARE(portPointer->applyRequests.size(), 1);
    QCOMPARE(portPointer->applyRequests.constFirst().scope,
             DisplayTransaction::ApplyScope::ForwardCandidate);
    const qsizetype signalsBeforeDeadline = changed.epochs.size();

    // The first deadline enters uncertainty observation; the re-armed second
    // deadline starts the bounded rollback request without replaying forward.
    QTRY_COMPARE_WITH_TIMEOUT(portPointer->applyRequests.size(), 2, 1'000);
    QCOMPARE(portPointer->applyRequests.constLast().scope,
             DisplayTransaction::ApplyScope::FullPreimage);
    QVERIFY(changed.epochs.size() > signalsBeforeDeadline);

    service.stop();
    QVERIFY(!service.isRunning());
    QVERIFY(!inventoryPointer->started);
    QVERIFY(inventoryPointer->observer == nullptr);
    QVERIFY(portPointer->observer == nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(
        !client.interface()->isServiceRegistered(
                               QString::fromLatin1(Display::kServiceName))
             .value(),
        5'000);

    QDBusPendingCallWatcher removedObjectWatcher(client.asyncCall(
        serviceCall(residentUniqueOwner, QStringLiteral("GetSnapshot")), 5'000));
    QTRY_VERIFY_WITH_TIMEOUT(removedObjectWatcher.isFinished(), 5'000);
    const QDBusPendingReply<Display::Snapshot> removedObjectReply(
        removedObjectWatcher);
    QVERIFY(removedObjectReply.isError());
    QCOMPARE(removedObjectReply.error().name(),
             QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"));

    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(residentName);
}

QTEST_MAIN(ResidentDisplayServicePrivateBusTest)

#include "tst_resident_display_service_private_bus.moc"
