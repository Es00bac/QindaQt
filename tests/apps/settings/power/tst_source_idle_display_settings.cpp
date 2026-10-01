// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_power/source_idle_display_settings.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include "power_settings_test_support.h"

#include <QtTest>

using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;
using namespace QindaQt::Apps::SettingsPower::TestSupport;

namespace {
const QString owner = QStringLiteral(":1.86");
const QString epoch = QStringLiteral("source-idle-settings-epoch");

QStringList scopedKeys()
{
    return {QStringLiteral("power.idle.ac.displayOffEnabled"),
            QStringLiteral("power.idle.ac.displayOffSeconds"),
            QStringLiteral("power.idle.battery.displayOffEnabled"),
            QStringLiteral("power.idle.battery.displayOffSeconds"),
            QStringLiteral("power.idle.lowBattery.displayOffEnabled"),
            QStringLiteral("power.idle.lowBattery.displayOffSeconds")};
}

QVariantMap settingsValues()
{
    return {{scopedKeys().at(0), true}, {scopedKeys().at(1), 600},
            {scopedKeys().at(2), true}, {scopedKeys().at(3), 900},
            {scopedKeys().at(4), false}, {scopedKeys().at(5), 0}};
}

QVariantMap snapshotWire(const quint64 revision, const QVariantMap &values)
{
    QVariantMap layers;
    for (const QString &key : scopedKeys())
        layers.insert(key, QStringLiteral("user-overrides"));
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), layers},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(const quint64 before, const quint64 after,
                       const QVariantMap &values, const QStringList &changed)
{
    const QString key = changed.constFirst();
    QVariantMap layers{{key, QStringLiteral("user-overrides")}};
    const QVariantMap changedValues{{key, values.value(key)}};
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), changedValues},
            {QLatin1StringView(WireContract::FieldSourceLayers), layers},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

class FakeSettingsTransport final : public SettingsTransport
{
public:
    struct SnapshotRequest { quint64 token; QString owner; };
    struct CommitRequest {
        quint64 token; QString owner; QString epoch; quint64 revision;
        QVariantList operations;
    };
    bool start(QString *error) override { if (error) error->clear(); return true; }
    void stop() override {}
    void requestSnapshot(const quint64 token, const QString &requestOwner,
                         const QStringList &) override
    { snapshots.push_back({token, requestOwner}); }
    void commit(const quint64 token, const QString &requestOwner,
                const QString &requestEpoch, const quint64 revision,
                const QVariantList &operations) override
    { commits.push_back({token, requestOwner, requestEpoch, revision, operations}); }
    void requestActivation() override {}
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

struct Fixture final {
    FakeSettingsTransport settingsTransport;
    SettingsClient settings{settingsTransport, scopedKeys(), ClientTiming{150, 0, {10}}};
    FakePowerTransport powerTransport;
    QindaQt::Power::PowerClient power{&powerTransport};
    SourceIdleDisplaySettingsModel model{power, settings};
    QVariantMap values = settingsValues();
    quint64 revision = 1;

    Fixture()
    {
        QVERIFY(settings.start());
        Q_EMIT settingsTransport.ownerChanged(owner);
        auto powerSnapshot = readySnapshot();
        powerSnapshot.source.acPresent = false;
        powerSnapshot.source.onBattery = true;
        publish(power, powerTransport, powerSnapshot);
    }
    bool answerSnapshot()
    {
        if (!QTest::qWaitFor([this] { return !settingsTransport.snapshots.isEmpty(); }, 2'000))
            return false;
        const auto request = settingsTransport.snapshots.takeFirst();
        if (request.owner != owner) return false;
        Q_EMIT settingsTransport.snapshotReceived(
            request.token, request.owner, snapshotWire(revision, values));
        return true;
    }
    bool ready() { return answerSnapshot() && model.available(); }
    void replyApplied(const QStringList &changed)
    {
        const auto request = settingsTransport.commits.constLast();
        Q_EMIT settingsTransport.commitReceived(
            request.token, request.owner,
            commitWire(request.revision, request.revision + 1, values, changed));
    }
};
} // namespace

class SourceIdleDisplaySettingsTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void confirmedSnapshotSelectsActiveSourceAndKeepsPerSourceValues();
    void activeProfileTracksPowerSourceTransitions();
    void appliedWriteRequiresExactFreshReadback();
    void invalidSourcesAndBoundsNeverReachSettings();
};

void SourceIdleDisplaySettingsTests::confirmedSnapshotSelectsActiveSourceAndKeepsPerSourceValues()
{
    Fixture fixture;
    QVERIFY(!fixture.model.available());
    QVERIFY(fixture.ready());
    QCOMPARE(fixture.model.activeSource(), QStringLiteral("lowBattery"));
    const QVariantList rows = fixture.model.sourceRows();
    QCOMPARE(rows.size(), 3);
    const QVariantMap ac = rows.at(0).toMap();
    const QVariantMap battery = rows.at(1).toMap();
    const QVariantMap lowBattery = rows.at(2).toMap();
    QCOMPARE(ac.value(QStringLiteral("seconds")).toInt(), 600);
    QCOMPARE(battery.value(QStringLiteral("seconds")).toInt(), 900);
    QVERIFY(lowBattery.value(QStringLiteral("active")).toBool());
    QVERIFY(!lowBattery.value(QStringLiteral("effectiveEnabled")).toBool());
    QVERIFY(ac.value(QStringLiteral("confirmed")).toBool());
}

void SourceIdleDisplaySettingsTests::activeProfileTracksPowerSourceTransitions()
{
    Fixture fixture;
    QVERIFY(fixture.ready());
    QCOMPARE(fixture.model.activeSource(), QStringLiteral("lowBattery"));

    const auto publishTransition = [&fixture](const QindaQt::Power::Snapshot &snapshot) {
        Q_EMIT fixture.powerTransport.invalidated(
            QStringLiteral(":1.80"), 41, snapshot.revision);
        const auto request = fixture.powerTransport.fetches.constLast();
        fixture.powerTransport.finishSnapshot(request.first, request.second, snapshot);
    };
    auto snapshot = readySnapshot(41, 8);
    snapshot.source.acPresent = true;
    snapshot.source.onBattery = false;
    snapshot.composite.warning = QindaQt::Power::WarningLevel::Action;
    publishTransition(snapshot);
    QCOMPARE(fixture.model.activeSource(), QStringLiteral("ac"));

    snapshot.revision = 9;
    snapshot.source.acPresent = false;
    snapshot.source.onBattery = true;
    snapshot.composite.warning = QindaQt::Power::WarningLevel::Unknown;
    publishTransition(snapshot);
    QCOMPARE(fixture.model.activeSource(), QStringLiteral("battery"));

    snapshot.revision = 10;
    snapshot.composite.warning = QindaQt::Power::WarningLevel::Critical;
    publishTransition(snapshot);
    QCOMPARE(fixture.model.activeSource(), QStringLiteral("lowBattery"));
}

void SourceIdleDisplaySettingsTests::appliedWriteRequiresExactFreshReadback()
{
    Fixture fixture;
    QVERIFY(fixture.ready());
    QVERIFY(fixture.model.setSeconds(QStringLiteral("ac"), 321));
    QVERIFY2(fixture.model.busy(), qPrintable(fixture.model.errorText()));
    QCOMPARE(fixture.settingsTransport.commits.size(), 1);
    const auto request = fixture.settingsTransport.commits.constFirst();
    QCOMPARE(request.operations.constFirst().toMap()
                 .value(QLatin1StringView(WireContract::FieldKey)).toString(),
             QStringLiteral("power.idle.ac.displayOffSeconds"));
    QCOMPARE(request.operations.constFirst().toMap()
                 .value(QLatin1StringView(WireContract::FieldValue)).toInt(), 321);

    fixture.values.insert(QStringLiteral("power.idle.ac.displayOffSeconds"), 321);
    fixture.revision = 2;
    fixture.replyApplied({QStringLiteral("power.idle.ac.displayOffSeconds")});
    QVERIFY2(fixture.model.busy(), qPrintable(fixture.model.errorText()));
    QVERIFY(fixture.model.statusText().contains(QStringLiteral("Checking")));
    fixture.settings.refresh();
    QVERIFY(fixture.answerSnapshot());
    QVERIFY(!fixture.model.busy());
    QCOMPARE(fixture.model.sourceRows().first().toMap()
                 .value(QStringLiteral("seconds")).toInt(), 321);
    QVERIFY(fixture.model.errorText().isEmpty());
    QCOMPARE(fixture.settingsTransport.commits.size(), 1);
}

void SourceIdleDisplaySettingsTests::invalidSourcesAndBoundsNeverReachSettings()
{
    Fixture fixture;
    QVERIFY(fixture.ready());
    QVERIFY(!fixture.model.setEnabled(QStringLiteral("unknown"), false));
    QVERIFY(!fixture.model.setSeconds(QStringLiteral("ac"), -1));
    QVERIFY(!fixture.model.setSeconds(QStringLiteral("ac"), 14401));
    QVERIFY(!fixture.model.setSeconds(QStringLiteral("ac"), 600));
    QCOMPARE(fixture.settingsTransport.commits.size(), 0);
}

QTEST_GUILESS_MAIN(SourceIdleDisplaySettingsTests)
#include "tst_source_idle_display_settings.moc"
