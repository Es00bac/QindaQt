// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        ++starts;
        if (!startSucceeds) {
            if (error != nullptr) *error = QStringLiteral("transport unavailable");
            return false;
        }
        started = true;
        if (error != nullptr) error->clear();
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
    int starts = 0;
    bool startSucceeds = true;
    bool started = false;
};

namespace {
QVariantMap snapshotWire(QString epoch, quint64 revision, bool enabled)
{
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues),
             QVariantMap{{QStringLiteral("services.doNotDisturb"), enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{QStringLiteral("services.doNotDisturb"), QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after, bool enabled)
{
    const QString key = QStringLiteral("services.doNotDisturb");
    const QStringList changed = status == SettingsWireStatus::Applied && after == before + 1
                                    ? QStringList{key} : QStringList{};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues),
             QVariantMap{{key, enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}
}

class SettingsClientTests final : public QObject {
    Q_OBJECT
private slots:
    void bindsSnapshotsAndFencesOwnerReplacement();
    void rejectsUnsupportedWireVersions();
    void rejectsOverAggregateSnapshots();
    void startFailurePublishesUnavailableAndRetryRecovers();
    void activationFailureUsesSerializedBackoffAndRecovers();
    void activationCompletionWithoutOwnerBacksOffAndRecovers();
    void rejectsEpochSwitchFromTheSameOwner();
    void timeoutAndBusLossMakeWritesUncertainWithoutReplay();
    void conflictIsConfirmedThenRefreshesAuthority();
    void rejectsContradictoryCommitReplies_data();
    void rejectsContradictoryCommitReplies();
    void invalidationsAreBoundedAndCannotCreateTargetRevisionLoops();
};

void SettingsClientTests::bindsSnapshotsAndFencesOwnerReplacement()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QVERIFY(client.start());
    QCOMPARE(transport.activations, 1);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.10"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto first = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(first.token, first.owner,
                                      snapshotWire(QStringLiteral("epoch-a"), 4, true));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QCOMPARE(client.snapshot()->values.value(QStringLiteral("services.doNotDisturb")).toBool(), true);

    Q_EMIT transport.ownerChanged(QStringLiteral(":1.11"));
    QCOMPARE(client.state(), ClientState::Authenticating);
    QVERIFY(client.snapshot().has_value()); // last confirmed value is retained
    QTRY_COMPARE(transport.snapshots.size(), 1);
    Q_EMIT transport.snapshotReceived(first.token, QStringLiteral(":1.10"),
                                      snapshotWire(QStringLiteral("epoch-a"), 99, false));
    QCOMPARE(client.state(), ClientState::Authenticating);
    const auto replacement = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(replacement.token, replacement.owner,
                                      snapshotWire(QStringLiteral("epoch-b"), 0, false));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QCOMPARE(client.snapshot()->owner, QStringLiteral(":1.11"));
    QCOMPARE(client.snapshot()->values.value(QStringLiteral("services.doNotDisturb")).toBool(), false);
}

void SettingsClientTests::rejectsUnsupportedWireVersions()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.12"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    auto unsupported = snapshotWire(QStringLiteral("epoch"), 0, false);
    unsupported.insert(QLatin1StringView(WireContract::FieldWireSchemaVersion), quint32(2));
    Q_EMIT transport.snapshotReceived(request.token, request.owner, unsupported);
    QCOMPARE(client.state(), ClientState::Degraded);
    QVERIFY(!client.snapshot().has_value());
}

void SettingsClientTests::rejectsOverAggregateSnapshots()
{
    QStringList keys;
    QVariantMap values;
    QVariantMap sources;
    const QVariantList largeValue(16, QString(16'000, QLatin1Char('x')));
    for (int index = 0; index < 5; ++index) {
        const QString key = QStringLiteral("test.key.%1").arg(index);
        keys.append(key);
        values.insert(key, largeValue);
        sources.insert(key, QStringLiteral("user-overrides"));
    }
    FakeTransport transport;
    SettingsClient client(transport, keys,
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.13"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    QVariantMap reply{{QLatin1StringView(WireContract::FieldStatus),
                       quint32(SettingsWireStatus::Applied)},
                      {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                       WireContract::WireSchemaVersion},
                      {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                      {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
                      {QLatin1StringView(WireContract::FieldRevision), quint64(0)},
                      {QLatin1StringView(WireContract::FieldValues), values},
                      {QLatin1StringView(WireContract::FieldSourceLayers), sources}};
    Q_EMIT transport.snapshotReceived(request.token, request.owner, reply);
    QCOMPARE(client.state(), ClientState::Degraded);
    QVERIFY(!client.snapshot().has_value());
}

void SettingsClientTests::startFailurePublishesUnavailableAndRetryRecovers()
{
    FakeTransport transport;
    transport.startSucceeds = false;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    DoNotDisturbController controller(client);
    QString error;
    QVERIFY(!client.start(&error));
    QCOMPARE(error, QStringLiteral("transport unavailable"));
    QCOMPARE(client.state(), ClientState::Unavailable);
    QVERIFY(controller.unavailable());
    QCOMPARE(transport.starts, 1);
    QCOMPARE(transport.activations, 0);

    controller.retry();
    QCOMPARE(transport.starts, 2);
    QCOMPARE(transport.activations, 0);
    QVERIFY(controller.unavailable());
    QCOMPARE(controller.errorText(), QStringLiteral("transport unavailable"));

    transport.startSucceeds = true;
    controller.retry();
    QCOMPARE(transport.starts, 3);
    QCOMPARE(transport.activations, 1);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.14"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch"), 0, false));
    QTRY_VERIFY(controller.ready());
}

void SettingsClientTests::activationFailureUsesSerializedBackoffAndRecovers()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {40, 80}});
    QVERIFY(client.start());
    QCOMPARE(transport.activations, 1);
    Q_EMIT transport.activationFailed(QStringLiteral("activation missing"));
    QCOMPARE(client.state(), ClientState::Unavailable);
    for (int index = 0; index < 20; ++index) {
        Q_EMIT transport.activationFailed(QStringLiteral("duplicate failure"));
        client.refresh();
    }
    QCOMPARE(transport.activations, 1);
    QTest::qWait(20);
    QCOMPARE(transport.activations, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.activations, 2, 100);

    Q_EMIT transport.activationFailed(QStringLiteral("still missing"));
    QTest::qWait(40);
    QCOMPARE(transport.activations, 2);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.15"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch"), 0, false));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QTest::qWait(60);
    QCOMPARE(transport.activations, 2);
}

void SettingsClientTests::activationCompletionWithoutOwnerBacksOffAndRecovers()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {40, 80}});
    DoNotDisturbController controller(client);
    QVERIFY(client.start());
    QCOMPARE(transport.activations, 1);

    Q_EMIT transport.activationCompleted();
    QVERIFY(controller.unavailable());
    QVERIFY(controller.errorText().contains(QStringLiteral("stable owner")));
    for (int index = 0; index < 12; ++index) {
        client.refresh();
        Q_EMIT transport.activationCompleted();
    }
    QCOMPARE(transport.activations, 1);
    QTest::qWait(20);
    QCOMPARE(transport.activations, 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport.activations, 2, 100);

    // Empty failure diagnostics must still move a pre-baseline controller to
    // honest Unavailable rather than preserving its initial Loading tuple.
    Q_EMIT transport.activationFailed(QString{});
    QVERIFY(controller.unavailable());
    QCOMPARE(controller.errorText(), QStringLiteral("settings activation failed"));
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.16"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch"), 0, false));
    QTRY_VERIFY(controller.ready());
    QTest::qWait(60);
    QCOMPARE(transport.activations, 2);
}

void SettingsClientTests::rejectsEpochSwitchFromTheSameOwner()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.17"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch-a"), 4, true));
    QTRY_VERIFY(client.state() == ClientState::Ready);

    client.refresh();
    QTRY_COMPARE(transport.snapshots.size(), 1);
    request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch-b"), 0, false));
    QCOMPARE(client.state(), ClientState::Degraded);
    QCOMPARE(client.snapshot()->epoch, QStringLiteral("epoch-a"));
    QCOMPARE(client.snapshot()->revision, quint64(4));
    QVERIFY(client.snapshot()->values
                .value(QStringLiteral("services.doNotDisturb")).toBool());

    QTRY_COMPARE_WITH_TIMEOUT(transport.snapshots.size(), 1, 100);
    request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch-a"), 4, false));
    QCOMPARE(client.state(), ClientState::Degraded);
    QVERIFY(client.snapshot()->values
                .value(QStringLiteral("services.doNotDisturb")).toBool());
}

void SettingsClientTests::timeoutAndBusLossMakeWritesUncertainWithoutReplay()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 10, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QSignalSpy uncertain(&client, &SettingsClient::commitUncertain);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.20"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    auto baseline = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(baseline.token, baseline.owner,
                                      snapshotWire(QStringLiteral("epoch"), 0, false));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QVERIFY(client.setUserValue(QStringLiteral("services.doNotDisturb"), true));
    QCOMPARE(transport.commits.size(), 1);
    QTest::qWait(15);
    QCOMPARE(uncertain.size(), 1);
    QVERIFY(!transport.snapshots.isEmpty());
    QCOMPARE(transport.commits.size(), 1); // never replayed
    auto resync = transport.snapshots.takeLast();
    transport.snapshots.clear();
    Q_EMIT transport.snapshotReceived(resync.token, resync.owner,
                                      snapshotWire(QStringLiteral("epoch"), 1, true));
    QTRY_VERIFY(client.state() == ClientState::Ready);

    QVERIFY(client.setUserValue(QStringLiteral("services.doNotDisturb"), false));
    Q_EMIT transport.busDisconnected();
    QCOMPARE(client.state(), ClientState::Unavailable);
    QCOMPARE(uncertain.size(), 2);
    QCOMPARE(transport.commits.size(), 2);
    QCOMPARE(client.snapshot()->values.value(QStringLiteral("services.doNotDisturb")).toBool(), true);
}

void SettingsClientTests::conflictIsConfirmedThenRefreshesAuthority()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QSignalSpy outcomes(&client, &SettingsClient::commitFinished);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.30"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    auto baseline = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(baseline.token, baseline.owner,
                                      snapshotWire(QStringLiteral("epoch"), 3, false));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QVERIFY(client.setUserValue(QStringLiteral("services.doNotDisturb"), true));
    const auto commit = transport.commits.constLast();
    Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                    commitWire(SettingsWireStatus::Conflict, 4, 4, false));
    QTRY_COMPARE(outcomes.size(), 1);
    const auto outcome = qvariant_cast<CommitOutcome>(outcomes.constFirst().constFirst());
    QVERIFY(outcome.status == SettingsWireStatus::Conflict);
    QTRY_COMPARE(transport.snapshots.size(), 1);
}

void SettingsClientTests::rejectsContradictoryCommitReplies_data()
{
    QTest::addColumn<QVariantMap>("reply");
    auto applied = commitWire(SettingsWireStatus::Applied, 3, 4, true);

    auto wrongEpoch = applied;
    wrongEpoch.insert(QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("other"));
    QTest::newRow("wrong-epoch") << wrongEpoch;

    auto wrongBase = applied;
    wrongBase.insert(QLatin1StringView(WireContract::FieldRevisionBefore), quint64(2));
    QTest::newRow("wrong-initiating-base") << wrongBase;

    auto skippedRevision = applied;
    skippedRevision.insert(QLatin1StringView(WireContract::FieldRevisionAfter), quint64(5));
    QTest::newRow("applied-skips-revision") << skippedRevision;

    auto extraValue = applied;
    auto values = extraValue.value(QLatin1StringView(WireContract::FieldValues)).toMap();
    values.insert(QStringLiteral("appearance.theme"), QStringLiteral("qinda-dark"));
    extraValue.insert(QLatin1StringView(WireContract::FieldValues), values);
    QTest::newRow("extra-current-value") << extraValue;

    auto extraSource = applied;
    auto sources = extraSource.value(QLatin1StringView(WireContract::FieldSourceLayers)).toMap();
    sources.insert(QStringLiteral("appearance.theme"), QStringLiteral("system-defaults"));
    extraSource.insert(QLatin1StringView(WireContract::FieldSourceLayers), sources);
    QTest::newRow("extra-current-source") << extraSource;

    auto duplicateChanged = applied;
    duplicateChanged.insert(QLatin1StringView(WireContract::FieldChangedKeys),
                            QStringList{QStringLiteral("services.doNotDisturb"),
                                        QStringLiteral("services.doNotDisturb")});
    QTest::newRow("duplicate-changed-key") << duplicateChanged;

    auto oversizedChanged = applied;
    QStringList oversized;
    for (int index = 0; index <= WireContract::MaximumChangedKeysPerSignal; ++index) {
        oversized.append(QStringLiteral("test.key.%1").arg(index));
    }
    oversizedChanged.insert(QLatin1StringView(WireContract::FieldChangedKeys), oversized);
    QTest::newRow("oversized-changed-keys") << oversizedChanged;

    auto extraField = applied;
    extraField.insert(QStringLiteral("surprise"), true);
    QTest::newRow("extra-top-level-field") << extraField;

    auto wrongSchema = applied;
    wrongSchema.insert(QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(3));
    QTest::newRow("changed-settings-schema") << wrongSchema;

    auto badMessage = applied;
    badMessage.insert(QLatin1StringView(WireContract::FieldMessage), quint32(7));
    QTest::newRow("non-string-message") << badMessage;

    auto contradictedConflict = commitWire(SettingsWireStatus::Conflict, 4, 4, false);
    contradictedConflict.insert(QLatin1StringView(WireContract::FieldChangedKeys),
                                QStringList{QStringLiteral("services.doNotDisturb")});
    QTest::newRow("conflict-claims-change") << contradictedConflict;

    auto impossibleEpochMismatch = commitWire(SettingsWireStatus::EpochMismatch, 3, 3, false);
    QTest::newRow("same-epoch-mismatch-status") << impossibleEpochMismatch;
}

void SettingsClientTests::rejectsContradictoryCommitReplies()
{
    QFETCH(QVariantMap, reply);
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QSignalSpy outcomes(&client, &SettingsClient::commitFinished);
    QSignalSpy uncertain(&client, &SettingsClient::commitUncertain);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.40"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto baseline = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(baseline.token, baseline.owner,
                                      snapshotWire(QStringLiteral("epoch"), 3, false));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QVERIFY(client.setUserValue(QStringLiteral("services.doNotDisturb"), true));
    const auto commit = transport.commits.constLast();
    Q_EMIT transport.commitReceived(commit.token, commit.owner, reply);
    QCOMPARE(outcomes.size(), 0);
    QCOMPARE(uncertain.size(), 1);
    QCOMPARE(client.state(), ClientState::Degraded);
    QTRY_COMPARE(transport.snapshots.size(), 1);
    QCOMPARE(transport.commits.size(), 1);
}

void SettingsClientTests::invalidationsAreBoundedAndCannotCreateTargetRevisionLoops()
{
    FakeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.50"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    auto baseline = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(baseline.token, baseline.owner,
                                      snapshotWire(QStringLiteral("epoch"), 3, false));
    QTRY_VERIFY(client.state() == ClientState::Ready);

    const QString key = QStringLiteral("services.doNotDisturb");
    Q_EMIT transport.settingsChanged(QStringLiteral(":1.50"), QStringLiteral("epoch"),
                                     std::numeric_limits<quint64>::max(),
                                     QStringList{QStringLiteral("appearance.theme")});
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto refresh = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner,
                                      snapshotWire(QStringLiteral("epoch"), 4, false));
    QTest::qWait(20);
    QVERIFY(transport.snapshots.isEmpty());

    Q_EMIT transport.settingsChanged(QStringLiteral(":1.50"), QStringLiteral("epoch"), 5,
                                     QStringList{key, key});
    QStringList oversized{key};
    for (int index = 1; index <= WireContract::MaximumChangedKeysPerSignal; ++index) {
        oversized.append(QStringLiteral("test.key.%1").arg(index));
    }
    Q_EMIT transport.settingsChanged(QStringLiteral(":1.50"), QStringLiteral("epoch"), 5,
                                     oversized);
    Q_EMIT transport.settingsChanged(QStringLiteral(":1.50"), QStringLiteral("epoch"), 5,
                                     QStringList{QString{}});
    QTest::qWait(20);
    QVERIFY(transport.snapshots.isEmpty());
}

QTEST_GUILESS_MAIN(SettingsClientTests)
#include "tst_settings_client.moc"
