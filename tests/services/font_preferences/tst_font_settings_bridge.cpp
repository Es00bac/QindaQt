// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_settings_bridge.h"
#include "qindaqt/services/font_preferences/font_preferences_codec.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::FontPreferences;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        if (!startSucceeds) {
            if (error != nullptr) *error = QStringLiteral("transport unavailable");
            return false;
        }
        started = true;
        return true;
    }
    void stop() override { started = false; }
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &keys) override
    { snapshots.append({token, owner, keys}); }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    { commits.append({token, owner, epoch, revision, operations}); }
    void requestActivation() override { ++activations; }

    struct SnapshotRequest { quint64 token; QString owner; QStringList keys; };
    struct CommitRequest { quint64 token; QString owner; QString epoch; quint64 revision; QVariantList operations; };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
    int activations = 0;
    bool startSucceeds = true;
    bool started = false;
};

} // namespace

namespace {

const ClientTiming TestTiming{100, 0, {10}};

QVariantMap fontsValues(const QString &family = QStringLiteral("Noto Sans"),
                        const QString &monospace = QStringLiteral("Noto Sans Mono"),
                        double pointSize = 10.0, bool antialiasing = true,
                        const QString &hinting = QStringLiteral("slight"),
                        const QString &subpixelOrder = QStringLiteral("rgb"))
{
    return {{QStringLiteral("fonts.family"), family},
            {QStringLiteral("fonts.monospaceFamily"), monospace},
            {QStringLiteral("fonts.pointSize"), pointSize},
            {QStringLiteral("fonts.antialiasing"), antialiasing},
            {QStringLiteral("fonts.hinting"), hinting},
            {QStringLiteral("fonts.subpixelOrder"), subpixelOrder}};
}

QVariantMap sourcesMap()
{
    QVariantMap sources;
    for (const QString &key : FontSettingsBridge::scopedKeys()) {
        sources.insert(key, QStringLiteral("user-overrides"));
    }
    return sources;
}

QVariantMap snapshotWire(const QString &epoch, quint64 revision, const QVariantMap &values)
{
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourcesMap()},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, const QString &epoch, quint64 before,
                       quint64 after, const QString &key, const QVariant &value)
{
    const QStringList changed = status == SettingsWireStatus::Applied && after == before + 1
                                    ? QStringList{key} : QStringList{};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

void driveBaseline(FakeTransport &transport, SettingsClient &client,
                   const QVariantMap &values, quint64 revision = 1)
{
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.60"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch-1"), revision, values));
    QTRY_VERIFY(client.state() == ClientState::Ready);
}

} // namespace

class FontSettingsBridgeTests final : public QObject {
    Q_OBJECT
private slots:
    void syncsConfirmedSnapshotAtomically();
    void invalidSnapshotValuesPreserveLastKnownGood();
    void wrongTypedSnapshotValuesAreRejectedWholesale();
    void writesAreRefusedWithoutBaseline();
    void invalidDraftIsRefused();
    void roundTripAppliesDraftThroughSettingsKeys();
    void conflictStopsSequenceWithoutReplay();
    void uncertainWriteIsNeverReplayed();
    void transportLossFailsClosed();
    void malformedPostCommitSnapshotStopsTheSequence();
};

void FontSettingsBridgeTests::syncsConfirmedSnapshotAtomically()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QSignalSpy synced(&bridge, &FontSettingsBridge::snapshotSynced);
    QVERIFY(client.start());

    driveBaseline(transport, client,
                  fontsValues(QStringLiteral("Liberation Mono"),
                              QStringLiteral("Liberation Mono"), 12.5, true,
                              QStringLiteral("full"), QStringLiteral("bgr")));

    QCOMPARE(synced.size(), 1);
    QVERIFY(bridge.hasBaseline());
    QVERIFY(bridge.lastSyncError().isEmpty());
    QCOMPARE(coordinator.preferences().family(), QStringLiteral("Liberation Mono"));
    QCOMPARE(coordinator.preferences().monospaceFamily(), QStringLiteral("Liberation Mono"));
    QCOMPARE(coordinator.preferences().pointSize(), 12.5);
    QCOMPARE(coordinator.preferences().hinting(), FontHinting::Full);
    QCOMPARE(coordinator.preferences().subpixelOrder(), FontSubpixelOrder::Bgr);
    QCOMPARE(coordinator.revision(), 2);
    QCOMPARE(coordinator.lastKnownGoodPreferences(), coordinator.preferences());
}

void FontSettingsBridgeTests::invalidSnapshotValuesPreserveLastKnownGood()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QVERIFY(client.start());
    driveBaseline(transport, client, fontsValues());
    QCOMPARE(coordinator.revision(), 1);

    Q_EMIT transport.settingsChanged(QStringLiteral(":1.60"), QStringLiteral("epoch-1"),
                                     2, FontSettingsBridge::scopedKeys());
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner,
        snapshotWire(QStringLiteral("epoch-1"), 2,
                     fontsValues(QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans Mono"),
                                 10.0, true, QStringLiteral("bogus-hinting"))));
    QTRY_VERIFY(!bridge.lastSyncError().isEmpty());
    // AGENT-GUARD: The hostile hinting value decodes to nothing; the
    // coordinator, its revision, and the LKG snapshot are all untouched.
    QCOMPARE(coordinator.preferences(), FontPreferences::systemDefaults());
    QCOMPARE(coordinator.revision(), 1);
    QCOMPARE(coordinator.lastKnownGoodPreferences(), FontPreferences::systemDefaults());
}

void FontSettingsBridgeTests::wrongTypedSnapshotValuesAreRejectedWholesale()
{
    // AGENT-NOTE: review finding P1-4 (rejected candidate abc76f3) — on the
    // unrepaired tree this wrong-typed authoritative snapshot was coerced
    // (family 123 -> "123", pointSize "12.5" -> 12.5) and published as the
    // confirmed baseline.
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QVERIFY(client.start());

    QVariantMap wrongTyped = fontsValues();
    wrongTyped.insert(QStringLiteral("fonts.family"), 123);
    wrongTyped.insert(QStringLiteral("fonts.pointSize"), QStringLiteral("12.5"));
    driveBaseline(transport, client, wrongTyped);

    QTRY_VERIFY(bridge.hasBaseline());
    QTRY_VERIFY(!bridge.lastSyncError().isEmpty());
    QCOMPARE(coordinator.preferences(), FontPreferences::systemDefaults());
    QCOMPARE(coordinator.revision(), 1);
    QCOMPARE(coordinator.lastKnownGoodPreferences(), FontPreferences::systemDefaults());
}

void FontSettingsBridgeTests::writesAreRefusedWithoutBaseline()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);

    FontPreferences draft;
    draft.setFamily(QStringLiteral("Liberation Mono"));
    QString error;
    QVERIFY(!bridge.applyPreferences(draft, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(transport.commits.isEmpty());
    QVERIFY(!bridge.applyInFlight());
}

void FontSettingsBridgeTests::invalidDraftIsRefused()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QVERIFY(client.start());
    driveBaseline(transport, client, fontsValues());

    FontPreferences draft;
    draft.setFamily(QString(QChar(0x01)));
    QVERIFY(!draft.isValid());
    QString error;
    QVERIFY(!bridge.applyPreferences(draft, &error));
    QVERIFY(transport.commits.isEmpty());
}

void FontSettingsBridgeTests::roundTripAppliesDraftThroughSettingsKeys()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QSignalSpy finished(&bridge, &FontSettingsBridge::applyFinished);
    QVERIFY(client.start());

    QVariantMap currentValues = fontsValues();
    quint64 revision = 1;
    driveBaseline(transport, client, currentValues, revision);

    FontPreferences draft;
    draft.setFamily(QStringLiteral("Liberation Mono"));
    draft.setMonospaceFamily(QStringLiteral("Liberation Mono"));
    draft.setPointSize(12.5);
    draft.setAntialiasing(FontAntialiasing::Subpixel);
    draft.setHinting(FontHinting::Full);
    draft.setSubpixelOrder(FontSubpixelOrder::Vrgb);
    QVERIFY(bridge.applyPreferences(draft));
    QVERIFY(bridge.applyInFlight());

    const QVariantMap draftMap = FontPreferencesCodec::toSettingsMap(draft);
    const QStringList order = FontSettingsBridge::scopedKeys();
    for (const QString &key : order) {
        QTRY_COMPARE(transport.commits.size(), 1);
        const auto commit = transport.commits.takeFirst();
        QCOMPARE(commit.revision, revision);
        QCOMPARE(commit.operations.size(), 1);
        QCOMPARE(commit.operations.first().toMap().value(QLatin1StringView(WireContract::FieldKey)).toString(),
                 key);
        const QVariant value = draftMap.value(key);
        Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                        commitWire(SettingsWireStatus::Applied,
                                                   QStringLiteral("epoch-1"), revision,
                                                   revision + 1, key, value));
        revision += 1;
        currentValues.insert(key, value);
        QTRY_COMPARE(transport.snapshots.size(), 1);
        const auto snapshot = transport.snapshots.takeFirst();
        Q_EMIT transport.snapshotReceived(snapshot.token, snapshot.owner,
                                          snapshotWire(QStringLiteral("epoch-1"), revision,
                                                       currentValues));
    }

    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!bridge.applyInFlight());
    QCOMPARE(transport.commits.size(), 0);
    QCOMPARE(bridge.lastApplyResults().size(), order.size());
    for (int i = 0; i < order.size(); ++i) {
        QCOMPARE(bridge.lastApplyResults().at(i).key, order.at(i));
        QCOMPARE(bridge.lastApplyResults().at(i).outcome, FontSettingsBridge::KeyOutcome::Applied);
    }
    // The confirmed snapshot after the sequence decodes back to the draft:
    // preferences round-trip through the documented Settings1 keys.
    QCOMPARE(coordinator.preferences(), draft);
}

void FontSettingsBridgeTests::conflictStopsSequenceWithoutReplay()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QSignalSpy finished(&bridge, &FontSettingsBridge::applyFinished);
    QVERIFY(client.start());

    const QVariantMap currentValues = fontsValues();
    driveBaseline(transport, client, currentValues, 1);

    FontPreferences draft;
    draft.setFamily(QStringLiteral("Liberation Mono"));
    QVERIFY(bridge.applyPreferences(draft));
    QTRY_COMPARE(transport.commits.size(), 1);
    const auto commit = transport.commits.takeFirst();
    const QString firstKey = FontSettingsBridge::scopedKeys().first();
    // The service moved to revision 7 meanwhile; the conflict reply is a
    // confirmed rejection, never a retry request.
    Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                    commitWire(SettingsWireStatus::Conflict,
                                               QStringLiteral("epoch-1"), 7, 7, firstKey,
                                               currentValues.value(firstKey)));

    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!bridge.applyInFlight());
    QCOMPARE(bridge.lastApplyResults().size(), 6);
    QCOMPARE(bridge.lastApplyResults().first().outcome, FontSettingsBridge::KeyOutcome::Conflict);
    for (int i = 1; i < 6; ++i) {
        QCOMPARE(bridge.lastApplyResults().at(i).outcome,
                 FontSettingsBridge::KeyOutcome::NotAttempted);
    }
    // The client refreshes its authority after the rejection; answer it and
    // confirm the bridge never replays the conflicted write.
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto snapshot = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(snapshot.token, snapshot.owner,
                                      snapshotWire(QStringLiteral("epoch-1"), 7, currentValues));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QCOMPARE(transport.commits.size(), 0);
}

void FontSettingsBridgeTests::uncertainWriteIsNeverReplayed()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QSignalSpy finished(&bridge, &FontSettingsBridge::applyFinished);
    QVERIFY(client.start());
    driveBaseline(transport, client, fontsValues(), 1);

    FontPreferences draft;
    draft.setFamily(QStringLiteral("Liberation Mono"));
    QVERIFY(bridge.applyPreferences(draft));
    QTRY_COMPARE(transport.commits.size(), 1);
    const auto commit = transport.commits.takeFirst();
    Q_EMIT transport.requestFailed(commit.token, commit.owner,
                                   QStringLiteral("org.qindaqt.Settings1.Error.Timeout"),
                                   QStringLiteral("commit timed out"));

    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(bridge.lastApplyResults().first().outcome, FontSettingsBridge::KeyOutcome::Uncertain);
    for (int i = 1; i < 6; ++i) {
        QCOMPARE(bridge.lastApplyResults().at(i).outcome,
                 FontSettingsBridge::KeyOutcome::NotAttempted);
    }
    // Recovery traffic must never contain another commit.
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto snapshot = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(snapshot.token, snapshot.owner,
                                      snapshotWire(QStringLiteral("epoch-1"), 1, fontsValues()));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QCOMPARE(transport.commits.size(), 0);
}

void FontSettingsBridgeTests::transportLossFailsClosed()
{
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QVERIFY(client.start());
    driveBaseline(transport, client,
                  fontsValues(QStringLiteral("Liberation Mono")), 1);
    QCOMPARE(coordinator.preferences().family(), QStringLiteral("Liberation Mono"));

    Q_EMIT transport.ownerChanged(QString{});
    QTRY_VERIFY(client.state() == ClientState::Unavailable);
    // AGENT-GUARD: Transport loss never claims last confirmed values are
    // current authority; the coordinator keeps its atomic LKG state.
    QCOMPARE(coordinator.preferences().family(), QStringLiteral("Liberation Mono"));
    QCOMPARE(coordinator.lastKnownGoodPreferences(), coordinator.preferences());

    FontPreferences draft;
    draft.setFamily(QStringLiteral("Noto Sans Ogham"));
    QString error;
    QVERIFY(!bridge.applyPreferences(draft, &error));
    QVERIFY(transport.commits.isEmpty());
}

void FontSettingsBridgeTests::malformedPostCommitSnapshotStopsTheSequence()
{
    // AGENT-NOTE: review finding P1-5 (rejected candidate abc76f3) — on the
    // unrepaired tree a malformed authoritative refresh after an Applied
    // commit still advanced the write sequence (nextCommits=1); the repair
    // ends the sequence with the remaining keys NotAttempted.
    FakeTransport transport;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(), TestTiming);
    FontPreferencesCoordinator coordinator;
    FontSettingsBridge bridge(client, coordinator);
    QSignalSpy finished(&bridge, &FontSettingsBridge::applyFinished);
    QVERIFY(client.start());

    QVariantMap currentValues = fontsValues();
    driveBaseline(transport, client, currentValues, 1);

    FontPreferences draft;
    draft.setFamily(QStringLiteral("Liberation Mono"));
    QVERIFY(bridge.applyPreferences(draft));
    QVERIFY(bridge.applyInFlight());
    QTRY_COMPARE(transport.commits.size(), 1);
    const auto commit = transport.commits.takeFirst();
    const QString firstKey = FontSettingsBridge::scopedKeys().first();
    Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                    commitWire(SettingsWireStatus::Applied,
                                               QStringLiteral("epoch-1"), 1, 2, firstKey,
                                               draft.family()));

    // The follow-up authoritative snapshot is malformed (wrong-typed value);
    // it must fence the sequence instead of launching the next key write.
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto snapshot = transport.snapshots.takeFirst();
    QVariantMap malformed = currentValues;
    malformed.insert(firstKey, draft.family());
    malformed.insert(QStringLiteral("fonts.hinting"), 42);
    Q_EMIT transport.snapshotReceived(snapshot.token, snapshot.owner,
                                      snapshotWire(QStringLiteral("epoch-1"), 2, malformed));

    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!bridge.applyInFlight());
    QVERIFY(!bridge.lastSyncError().isEmpty());
    QCOMPARE(transport.commits.size(), 0);
    QCOMPARE(bridge.lastApplyResults().size(), 6);
    QCOMPARE(bridge.lastApplyResults().first().key, firstKey);
    QCOMPARE(bridge.lastApplyResults().first().outcome, FontSettingsBridge::KeyOutcome::Applied);
    for (int i = 1; i < 6; ++i) {
        QCOMPARE(bridge.lastApplyResults().at(i).outcome,
                 FontSettingsBridge::KeyOutcome::NotAttempted);
    }
    // The coordinator keeps its last-known-good preferences throughout.
    QCOMPARE(coordinator.preferences(), FontPreferences::systemDefaults());
}

QTEST_GUILESS_MAIN(FontSettingsBridgeTests)
#include "tst_font_settings_bridge.moc"
