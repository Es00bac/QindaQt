// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_upower_service.h"
#include "support/private_bus.h"

#include <qindaqt/services/power_service/adapters/production_battery_collaborator.h>
#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>
#include <qindaqt/services/power_service/adapters/upower_battery_collaborator.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

void writeFixture(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
}

QByteArray readFixture(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void makePanel(const QString &root, const QString &name, const QByteArray &type,
               const QByteArray &brightness, const QByteArray &actual = QByteArray())
{
    const QString directory = root + QLatin1Char('/') + name;
    writeFixture(directory + QStringLiteral("/type"), type);
    writeFixture(directory + QStringLiteral("/max_brightness"), QByteArrayLiteral("255\n"));
    writeFixture(directory + QStringLiteral("/brightness"), brightness);
    if (!actual.isNull()) {
        writeFixture(directory + QStringLiteral("/actual_brightness"), actual);
    }
}

FakeUpowerService::DeviceSpec fixtureBattery()
{
    QVariantMap properties;
    properties.insert(QStringLiteral("Type"), QVariant(uint(2)));
    properties.insert(QStringLiteral("PowerSupply"), QVariant(true));
    properties.insert(QStringLiteral("IsPresent"), QVariant(true));
    properties.insert(QStringLiteral("State"), QVariant(uint(2)));
    properties.insert(QStringLiteral("Percentage"), QVariant(42.5));
    properties.insert(QStringLiteral("BatteryLevel"), QVariant(uint(1)));
    return {QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0"), properties};
}

// The production battery seam over a private UPower fake and an injected
// fixture root. Nothing here reaches the host bus, /sys, or hardware.
struct ApplyRow
{
    QTemporaryDir root{QStringLiteral(QINDAQT_TEST_SCRATCH_DIR)
                       + QStringLiteral("/internal-apply-XXXXXX")};
    PrivateBus bus;
    std::unique_ptr<FakeUpowerService> fake;
    std::unique_ptr<Upstream::ProductionBatteryCollaborator> production;
    std::unique_ptr<QSignalSpy> facts;
    std::unique_ptr<QSignalSpy> finished;
    quint64 generation = 0;

    bool start()
    {
        if (!root.isValid() || !bus.start()) {
            return false;
        }
        fake = std::make_unique<FakeUpowerService>(
            bus.openConnection(QStringLiteral("fake")));
        if (!fake->registerService()) {
            return false;
        }
        fake->setDevices({fixtureBattery()});
        fake->setOnBattery(true);
        production = std::make_unique<Upstream::ProductionBatteryCollaborator>(
            std::make_unique<Upstream::UpowerBatteryCollaborator>(bus.connection),
            std::make_unique<Upstream::SysfsBacklightSource>(root.path()));
        facts = std::make_unique<QSignalSpy>(production.get(),
                                             &BatteryCollaborator::factsChanged);
        finished = std::make_unique<QSignalSpy>(production.get(),
                                                &BatteryCollaborator::operationFinished);
        generation = production->start();
        return QTest::qWaitFor([this] { return !facts->isEmpty(); }, 5000);
    }

    [[nodiscard]] QString path(const QString &name, const QString &file) const
    {
        return root.path() + QLatin1Char('/') + name + QLatin1Char('/') + file;
    }

    [[nodiscard]] InternalBacklight backlight(const QString &name) const
    {
        const BatteryFacts latest = facts->constLast().at(1).value<BatteryFacts>();
        for (const InternalBacklight &device : latest.internalBacklights) {
            if (device.deviceName == name) {
                return device;
            }
        }
        return {};
    }

    [[nodiscard]] CollaboratorOutcome outcome(const qsizetype index) const
    {
        return finished->at(index).at(2).value<CollaboratorOutcome>();
    }
};

} // namespace

class PowerInternalBacklightApplyTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesOnALaterTurnAndPublishesReadbackFirst();
    void readbackKeepsTheKernelObservedValue();
    void unselectedAndAmbiguousTargetsAreNeverWritten();
    void readOnlyWriteFailsClosedWithoutUpstreamText();
    void withdrawnBatteryDomainRefusesTheWrite();
    void stoppedRunDropsAQueuedWrite();
    void queuedWritesApplyOneAtATimeInOrder();
};

void PowerInternalBacklightApplyTests::appliesOnALaterTurnAndPublishesReadbackFirst()
{
    QStringList events;
    ApplyRow row;
    makePanel(row.root.path(), QStringLiteral("panel"), "firmware\n", "128\n");
    QVERIFY(row.start());
    const Handle panel = row.backlight(QStringLiteral("panel")).handle;
    QVERIFY(!panel.opaqueId.isEmpty());

    const auto factsConnection = QObject::connect(
        row.production.get(), &BatteryCollaborator::factsChanged,
        [&events](quint64, const BatteryFacts &facts) {
            events.push_back(QStringLiteral("facts:%1").arg(
                facts.internalBacklights.constFirst().observed));
        });
    const auto finishedConnection = QObject::connect(
        row.production.get(), &BatteryCollaborator::operationFinished,
        [&events](quint64, const quint64 operationId, const CollaboratorOutcome &outcome) {
            events.push_back(
                QStringLiteral("finished:%1:%2").arg(operationId).arg(outcome.reasonCode));
        });

    row.production->submitSetInternalBrightness(41, panel, 200);
    // AGENT-GUARD: nothing is written or completed inside the submit call; the
    // D-Bus object registers its delayed reply only after submit returns.
    QVERIFY(events.isEmpty());
    QCOMPARE(readFixture(row.path(QStringLiteral("panel"), QStringLiteral("brightness"))),
             QByteArrayLiteral("128\n"));

    QTRY_COMPARE(row.finished->size(), 1);
    QObject::disconnect(factsConnection);
    QObject::disconnect(finishedConnection);
    QCOMPARE(readFixture(row.path(QStringLiteral("panel"), QStringLiteral("brightness"))),
             QByteArrayLiteral("200\n"));
    const qsizetype readback = events.indexOf(QStringLiteral("facts:200"));
    const qsizetype completion = events.indexOf(QStringLiteral("finished:41:applied"));
    QVERIFY2(readback >= 0 && completion > readback, qPrintable(events.join(u',')));
    QCOMPARE(row.finished->constFirst().at(0).toULongLong(), row.generation);
    QCOMPARE(row.outcome(0).status, CollaboratorStatus::Succeeded);
    QCOMPARE(row.backlight(QStringLiteral("panel")).observed, quint32(200));
}

void PowerInternalBacklightApplyTests::readbackKeepsTheKernelObservedValue()
{
    ApplyRow row;
    makePanel(row.root.path(), QStringLiteral("panel"), "raw\n", "128\n", "129\n");
    QVERIFY(row.start());
    QCOMPARE(row.backlight(QStringLiteral("panel")).observed, quint32(129));

    row.production->submitSetInternalBrightness(
        7, row.backlight(QStringLiteral("panel")).handle, 200);
    QTRY_COMPARE(row.finished->size(), 1);
    QCOMPARE(row.outcome(0).status, CollaboratorStatus::Succeeded);
    QCOMPARE(readFixture(row.path(QStringLiteral("panel"), QStringLiteral("brightness"))),
             QByteArrayLiteral("200\n"));
    // actual_brightness is the observation; the request never substitutes for it.
    QCOMPARE(row.backlight(QStringLiteral("panel")).observed, quint32(129));
}

void PowerInternalBacklightApplyTests::unselectedAndAmbiguousTargetsAreNeverWritten()
{
    ApplyRow row;
    makePanel(row.root.path(), QStringLiteral("edp"), "firmware\n", "10\n");
    makePanel(row.root.path(), QStringLiteral("dsi"), "firmware\n", "20\n");
    makePanel(row.root.path(), QStringLiteral("raw"), "raw\n", "30\n");
    QVERIFY(row.start());

    row.production->submitSetInternalBrightness(
        1, row.backlight(QStringLiteral("edp")).handle, 100);
    row.production->submitSetInternalBrightness(
        2, row.backlight(QStringLiteral("raw")).handle, 100);
    row.production->submitSetInternalBrightness(
        3, {.epoch = 0, .opaqueId = QStringLiteral("unknown")}, 100);
    QTRY_COMPARE(row.finished->size(), 3);
    for (qsizetype index = 0; index < 3; ++index) {
        QCOMPARE(row.outcome(index).status, CollaboratorStatus::Unsupported);
        QCOMPARE(row.outcome(index).reasonCode, QStringLiteral("backlight-not-admitted"));
    }
    QCOMPARE(readFixture(row.path(QStringLiteral("edp"), QStringLiteral("brightness"))),
             QByteArrayLiteral("10\n"));
    QCOMPARE(readFixture(row.path(QStringLiteral("dsi"), QStringLiteral("brightness"))),
             QByteArrayLiteral("20\n"));
    QCOMPARE(readFixture(row.path(QStringLiteral("raw"), QStringLiteral("brightness"))),
             QByteArrayLiteral("30\n"));
}

void PowerInternalBacklightApplyTests::readOnlyWriteFailsClosedWithoutUpstreamText()
{
    ApplyRow row;
    makePanel(row.root.path(), QStringLiteral("panel"), "firmware\n", "10\n");
    QVERIFY(row.start());
    QCOMPARE(row.backlight(QStringLiteral("panel")).status, BacklightStatus::Ok);

    // Permission changes after publication, as a systemd sandbox or udev rule
    // change would; the adapter must not route around the denied write.
    QFile brightness(row.path(QStringLiteral("panel"), QStringLiteral("brightness")));
    const QFileDevice::Permissions original = brightness.permissions();
    QVERIFY(brightness.setPermissions(QFileDevice::ReadOwner));
    row.production->submitSetInternalBrightness(
        5, row.backlight(QStringLiteral("panel")).handle, 50);
    // Deliver only the queued apply, before the watcher's attribute event can
    // republish the panel read-only: the race in which the published snapshot
    // still admitted the write.
    QCoreApplication::sendPostedEvents(row.production.get(), QEvent::MetaCall);
    QCOMPARE(row.finished->size(), 1);
    brightness.setPermissions(original);
    QCOMPARE(row.outcome(0).status, CollaboratorStatus::Failed);
    QCOMPARE(row.outcome(0).reasonCode, QStringLiteral("backlight-read-only"));
    QVERIFY(row.outcome(0).diagnostic.isEmpty());
    QCOMPARE(readFixture(row.path(QStringLiteral("panel"), QStringLiteral("brightness"))),
             QByteArrayLiteral("10\n"));
}

void PowerInternalBacklightApplyTests::withdrawnBatteryDomainRefusesTheWrite()
{
    ApplyRow row;
    makePanel(row.root.path(), QStringLiteral("panel"), "firmware\n", "10\n");
    QVERIFY(row.start());
    const Handle panel = row.backlight(QStringLiteral("panel")).handle;
    QSignalSpy unavailable(row.production.get(), &BatteryCollaborator::statusUnavailable);

    row.fake->unregisterService();
    QTRY_VERIFY(unavailable.size() >= 1);
    row.production->submitSetInternalBrightness(6, panel, 100);
    QTRY_COMPARE(row.finished->size(), 1);
    QCOMPARE(row.outcome(0).status, CollaboratorStatus::Unsupported);
    QCOMPARE(row.outcome(0).reasonCode, QStringLiteral("backlight-domain-unavailable"));
    QCOMPARE(readFixture(row.path(QStringLiteral("panel"), QStringLiteral("brightness"))),
             QByteArrayLiteral("10\n"));
}

void PowerInternalBacklightApplyTests::stoppedRunDropsAQueuedWrite()
{
    ApplyRow row;
    makePanel(row.root.path(), QStringLiteral("panel"), "firmware\n", "10\n");
    QVERIFY(row.start());
    row.production->submitSetInternalBrightness(
        8, row.backlight(QStringLiteral("panel")).handle, 100);
    // The coordinator completes a stopped run's operations Uncertain itself; a
    // queued write from that run must never execute afterwards.
    row.production->stop();
    QTest::qWait(100);
    QCOMPARE(row.finished->size(), 0);
    QCOMPARE(readFixture(row.path(QStringLiteral("panel"), QStringLiteral("brightness"))),
             QByteArrayLiteral("10\n"));
}

void PowerInternalBacklightApplyTests::queuedWritesApplyOneAtATimeInOrder()
{
    ApplyRow row;
    makePanel(row.root.path(), QStringLiteral("panel"), "firmware\n", "10\n");
    QVERIFY(row.start());
    const Handle panel = row.backlight(QStringLiteral("panel")).handle;
    row.production->submitSetInternalBrightness(11, panel, 60);
    row.production->submitSetInternalBrightness(12, panel, 70);
    QTRY_COMPARE(row.finished->size(), 2);
    QCOMPARE(row.finished->at(0).at(1).toULongLong(), quint64(11));
    QCOMPARE(row.finished->at(1).at(1).toULongLong(), quint64(12));
    QCOMPARE(row.outcome(0).status, CollaboratorStatus::Succeeded);
    QCOMPARE(row.outcome(1).status, CollaboratorStatus::Succeeded);
    QCOMPARE(readFixture(row.path(QStringLiteral("panel"), QStringLiteral("brightness"))),
             QByteArrayLiteral("70\n"));
    QCOMPARE(row.backlight(QStringLiteral("panel")).observed, quint32(70));
}

QTEST_GUILESS_MAIN(PowerInternalBacklightApplyTests)
#include "tst_power_internal_backlight_apply.moc"
