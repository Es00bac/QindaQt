// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { started = true; return true; }
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
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues),
             QVariantMap{{QStringLiteral("services.doNotDisturb"), enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{QStringLiteral("services.doNotDisturb"), QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys),
             QStringList{QStringLiteral("services.doNotDisturb")}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}
}

class SettingsClientTests final : public QObject {
    Q_OBJECT
private slots:
    void bindsSnapshotsAndFencesOwnerReplacement();
    void rejectsUnsupportedWireVersions();
    void rejectsOverAggregateSnapshots();
    void timeoutAndBusLossMakeWritesUncertainWithoutReplay();
    void conflictIsConfirmedThenRefreshesAuthority();
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
                                    commitWire(SettingsWireStatus::Conflict, 3, 4, false));
    QTRY_COMPARE(outcomes.size(), 1);
    const auto outcome = qvariant_cast<CommitOutcome>(outcomes.constFirst().constFirst());
    QVERIFY(outcome.status == SettingsWireStatus::Conflict);
    QTRY_COMPARE(transport.snapshots.size(), 1);
}

QTEST_GUILESS_MAIN(SettingsClientTests)
#include "tst_settings_client.moc"
