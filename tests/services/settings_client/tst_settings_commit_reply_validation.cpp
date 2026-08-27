// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QSignalSpy>
#include <QtTest>

#include <utility>

using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    struct SnapshotRequest final { quint64 token; QString owner; QStringList keys; };
    struct CommitRequest final {
        quint64 token;
        QString owner;
        QString epoch;
        quint64 revision;
        QVariantList operations;
    };

    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override
    {
        snapshots.append({token, owner, keys});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, revision, operations});
    }
    void requestActivation() override {}

    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

QVariantMap baselineWire(const QString &key)
{
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevision), quint64(3)},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, false}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("system-defaults")}}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap unknownWire(QVariantMap values = {}, QVariantMap sources = {})
{
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::UnknownKey)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), quint64(3)},
            {QLatin1StringView(WireContract::FieldRevisionAfter), quint64(3)},
            {QLatin1StringView(WireContract::FieldValues), std::move(values)},
            {QLatin1StringView(WireContract::FieldSourceLayers), std::move(sources)},
            {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{}},
            {QLatin1StringView(WireContract::FieldMessage),
             QStringLiteral("unknown key: unknown.key")}};
}

void establishBaseline(FakeTransport &transport, SettingsClient &client)
{
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.70"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner, baselineWire(QStringLiteral("unknown.key")));
    QTRY_VERIFY(client.state() == ClientState::Ready);
}

} // namespace

class SettingsCommitReplyValidationTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void acceptsExactUnknownKeyAuthorityShape_data();
    void acceptsExactUnknownKeyAuthorityShape();
    void rejectsNonEmptyUnknownKeyAuthority_data();
    void rejectsNonEmptyUnknownKeyAuthority();
};

void SettingsCommitReplyValidationTests::acceptsExactUnknownKeyAuthorityShape_data()
{
    QTest::addColumn<bool>("remove");
    QTest::newRow("set") << false;
    QTest::newRow("remove") << true;
}

void SettingsCommitReplyValidationTests::acceptsExactUnknownKeyAuthorityShape()
{
    QFETCH(bool, remove);
    const QString key = QStringLiteral("unknown.key");
    FakeTransport transport;
    SettingsClient client(transport, {key},
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QSignalSpy outcomes(&client, &SettingsClient::commitFinished);
    QSignalSpy uncertain(&client, &SettingsClient::commitUncertain);
    QVERIFY(client.start());
    establishBaseline(transport, client);

    QVERIFY(remove ? client.removeUserValue(key) : client.setUserValue(key, true));
    QCOMPARE(transport.commits.size(), 1);
    const auto commit = transport.commits.constFirst();
    QCOMPARE(commit.owner, QStringLiteral(":1.70"));
    QCOMPARE(commit.epoch, QStringLiteral("epoch"));
    QCOMPARE(commit.revision, quint64(3));
    const QVariantMap operation = commit.operations.constFirst().toMap();
    QCOMPARE(operation.value(QLatin1StringView(WireContract::FieldKey)).toString(), key);
    QCOMPARE(operation.value(QLatin1StringView(WireContract::FieldKind)).toString(),
             remove ? QLatin1String(WireContract::OperationKindRemove)
                    : QLatin1String(WireContract::OperationKindSet));

    Q_EMIT transport.commitReceived(commit.token, commit.owner, unknownWire());
    QTRY_COMPARE(outcomes.size(), 1);
    QCOMPARE(uncertain.size(), 0);
    const auto outcome = qvariant_cast<CommitOutcome>(outcomes.constFirst().constFirst());
    QVERIFY(outcome.status == SettingsWireStatus::UnknownKey);
    QCOMPARE(outcome.revisionBefore, quint64(3));
    QCOMPARE(outcome.revisionAfter, quint64(3));
    QVERIFY(outcome.currentValues.isEmpty());
    QVERIFY(outcome.currentSourceLayers.isEmpty());
    QVERIFY(outcome.changedKeys.isEmpty());
    QVERIFY(outcome.message.contains(key));
    QTRY_COMPARE(transport.snapshots.size(), 1);
}

void SettingsCommitReplyValidationTests::rejectsNonEmptyUnknownKeyAuthority_data()
{
    QTest::addColumn<bool>("includeValue");
    QTest::addColumn<bool>("includeSource");
    QTest::newRow("value-only") << true << false;
    QTest::newRow("source-only") << false << true;
    QTest::newRow("fabricated-pair") << true << true;
}

void SettingsCommitReplyValidationTests::rejectsNonEmptyUnknownKeyAuthority()
{
    QFETCH(bool, includeValue);
    QFETCH(bool, includeSource);
    const QString key = QStringLiteral("unknown.key");
    FakeTransport transport;
    SettingsClient client(transport, {key},
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QSignalSpy outcomes(&client, &SettingsClient::commitFinished);
    QSignalSpy uncertain(&client, &SettingsClient::commitUncertain);
    QVERIFY(client.start());
    establishBaseline(transport, client);
    QVERIFY(client.setUserValue(key, true));
    const auto commit = transport.commits.constFirst();
    QVariantMap values;
    QVariantMap sources;
    if (includeValue) values.insert(key, false);
    if (includeSource) sources.insert(key, QStringLiteral("system-defaults"));
    Q_EMIT transport.commitReceived(
        commit.token, commit.owner, unknownWire(std::move(values), std::move(sources)));
    QCOMPARE(outcomes.size(), 0);
    QCOMPARE(uncertain.size(), 1);
    QCOMPARE(client.state(), ClientState::Degraded);
    QTRY_COMPARE(transport.snapshots.size(), 1);
    QCOMPARE(transport.commits.size(), 1);
}

QTEST_GUILESS_MAIN(SettingsCommitReplyValidationTests)
#include "tst_settings_commit_reply_validation.moc"
