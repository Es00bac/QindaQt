// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_color_assignment/assignment_document.h>
#include <qindaqt/services/display_color_assignment/assignment_store.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::DisplayColor;
using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::SettingsClient;
using QindaQt::Services::SettingsClient::SettingsTransport;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using WC = QindaQt::Services::SettingsProtocol::WireContract;

namespace
{

constexpr auto kKey = "displays.colorAssignments";

QVariant assignmentValue(const QString &profileId, const QString &lineage)
{
    return QVariantMap{
        {QStringLiteral("DP-1"),
         QVariantMap{{QStringLiteral("profile"), profileId},
                     {QStringLiteral("lineage"), lineage}}}};
}

QVariantMap snapshotWire(const QString &epoch, quint64 revision, const QVariant &assignments)
{
    return {{QLatin1StringView(WC::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WC::FieldWireSchemaVersion), WC::WireSchemaVersion},
            {QLatin1StringView(WC::FieldSettingsSchemaVersion), quint32{2}},
            {QLatin1StringView(WC::FieldEpoch), epoch},
            {QLatin1StringView(WC::FieldRevision), revision},
            {QLatin1StringView(WC::FieldValues),
             QVariantMap{{QLatin1String(kKey), assignments}}},
            {QLatin1StringView(WC::FieldSourceLayers),
             QVariantMap{{QLatin1String(kKey), QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WC::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after,
                       const QVariant &authoritativeValue, const QStringList &changed)
{
    return {{QLatin1StringView(WC::FieldStatus), quint32(status)},
            {QLatin1StringView(WC::FieldWireSchemaVersion), WC::WireSchemaVersion},
            {QLatin1StringView(WC::FieldSettingsSchemaVersion), quint32{2}},
            {QLatin1StringView(WC::FieldEpoch), QStringLiteral("epoch-a")},
            {QLatin1StringView(WC::FieldRevisionBefore), before},
            {QLatin1StringView(WC::FieldRevisionAfter), after},
            {QLatin1StringView(WC::FieldValues),
             QVariantMap{{QLatin1String(kKey), authoritativeValue}}},
            {QLatin1StringView(WC::FieldSourceLayers),
             QVariantMap{{QLatin1String(kKey), QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WC::FieldChangedKeys), changed},
            {QLatin1StringView(WC::FieldMessage), QString{}}};
}

class FakeTransport final : public SettingsTransport
{
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        if (!startSucceeds) {
            if (error != nullptr) {
                *error = QStringLiteral("transport unavailable");
            }
            return false;
        }
        return true;
    }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &keys) override
    {
        snapshots.append({token, owner, keys});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch, quint64 baseRevision,
                const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, baseRevision, operations});
    }
    void requestActivation() override { ++activations; }

    struct SnapshotRequest
    {
        quint64 token;
        QString owner;
        QStringList keys;
    };
    struct CommitRequest
    {
        quint64 token;
        QString owner;
        QString epoch;
        quint64 revision;
        QVariantList operations;
    };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
    int activations = 0;
    bool startSucceeds = true;
};

QindaQt::Services::SettingsClient::ClientTiming testTiming()
{
    QindaQt::Services::SettingsClient::ClientTiming timing;
    timing.requestTimeoutMilliseconds = 120;
    timing.debounceMilliseconds = 0;
    timing.retryMilliseconds = {10};
    return timing;
}

} // namespace

class AssignmentStoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void readyViewDecodesTheConfirmedSnapshot();
    void rejectsWritesWithoutConfirmedAuthority();
    void applySendsOptimisticCommitAndReportsApplied();
    void appliedNoOpIsDistinguishedFromChange();
    void conflictIsReportedAndNeverReplayed();
    void uncertainTimeoutIsTerminalWithoutReplay();
    void hostilePersistedDocumentFailsClosed();
    void documentBecomesUnavailableOnTransportLoss();

private:
    void bringReady(FakeTransport &transport, SettingsClient &client, const QVariant &assignments);
};

void AssignmentStoreTests::bringReady(FakeTransport &transport, SettingsClient &client,
                                      const QVariant &assignments)
{
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.42"));
    // The client may hold several queued refresh requests after a retry
    // loop; only the newest carries the pending token the client accepts.
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto request = transport.snapshots.takeLast();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch-a"), 4, assignments));
    QTRY_VERIFY(client.state() == ClientState::Ready);
}

void AssignmentStoreTests::readyViewDecodesTheConfirmedSnapshot()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);

    QSignalSpy changedSpy(&store, &SettingsAssignmentStore::documentChanged);
    bringReady(transport, client,
               assignmentValue(QStringLiteral("vendor-srgb"),
                               QString::fromLatin1(
                                   "00112233445566778899aabbccddeeff"
                                   "00112233445566778899aabbccddeeff")));

    const AssignmentDocumentView view = store.document();
    QCOMPARE(view.availability, DocumentAvailability::Ready);
    QCOMPARE(view.epoch, QStringLiteral("epoch-a"));
    QCOMPARE(view.revision, quint64{4});
    QCOMPARE(view.document.records.size(), 1);
    QCOMPARE(view.document.records.first().profileId, QStringLiteral("vendor-srgb"));
    QVERIFY(changedSpy.count() >= 1);
}

void AssignmentStoreTests::rejectsWritesWithoutConfirmedAuthority()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);

    ColorAssignmentDraft draft;
    draft.entries.append({QStringLiteral("DP-1"), QStringLiteral("p"), QByteArray(), false});

    QString error;
    QVERIFY(!store.applyDraft(draft, &error));
    QCOMPARE(error, QStringLiteral("unavailable"));

    bringReady(transport, client, assignmentValue(QStringLiteral("p"), QString()));

    ColorAssignmentDraft invalid;
    invalid.entries.append({QStringLiteral("bad id"), QStringLiteral("p"), QByteArray(), false});
    QVERIFY(!store.applyDraft(invalid, &error));
    QVERIFY(error.startsWith(QStringLiteral("invalid-draft/")));
    QCOMPARE(store.writeInFlight(), false);
    QVERIFY(transport.commits.isEmpty());
}

void AssignmentStoreTests::applySendsOptimisticCommitAndReportsApplied()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);
    bringReady(transport, client, assignmentValue(QStringLiteral("p"), QString()));

    ColorAssignmentDraft draft;
    draft.entries.append(
        {QStringLiteral("DP-1"), QStringLiteral("better"), QByteArray(), false});
    QString error;
    QVERIFY(store.applyDraft(draft, &error));
    QCOMPARE(store.writeInFlight(), true);
    QCOMPARE(transport.commits.size(), 1);
    // The optimistic commit is fenced with the confirmed snapshot revision.
    QCOMPARE(transport.commits.first().revision, quint64{4});
    QCOMPARE(transport.commits.first().epoch, QStringLiteral("epoch-a"));

    QSignalSpy finishedSpy(&store, &SettingsAssignmentStore::applyFinished);
    const QVariant merged = assignmentValue(QStringLiteral("better"), QString());
    Q_EMIT transport.commitReceived(transport.commits.first().token,
                                    transport.commits.first().owner,
                                    commitWire(SettingsWireStatus::Applied, 4, 5, merged,
                                               QStringList{QLatin1String(kKey)}));

    QTRY_VERIFY(!finishedSpy.isEmpty());
    const auto outcome = finishedSpy.first().first().value<AssignmentApplyOutcome>();
    QCOMPARE(outcome.status, ApplyStatus::Applied);
    QCOMPARE(outcome.revisionAfter, quint64{5});
    QCOMPARE(outcome.persistedDocument.records.first().profileId, QStringLiteral("better"));
    QCOMPARE(store.writeInFlight(), false);

    // The client refreshes authority after the commit; the fresh snapshot
    // shows the persisted document.
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto refresh = transport.snapshots.takeLast();
    Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner,
                                      snapshotWire(QStringLiteral("epoch-a"), 5, merged));
    QTRY_VERIFY(store.document().availability == DocumentAvailability::Ready);
    QCOMPARE(store.document().revision, quint64{5});
    QCOMPARE(store.document().document.records.first().profileId, QStringLiteral("better"));
}

void AssignmentStoreTests::appliedNoOpIsDistinguishedFromChange()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);
    const QVariant current = assignmentValue(QStringLiteral("p"), QString());
    bringReady(transport, client, current);

    // Re-assigning the exact current record produces the identical document.
    ColorAssignmentDraft draft;
    draft.entries.append({QStringLiteral("DP-1"), QStringLiteral("p"), QByteArray(), false});
    QString error;
    QVERIFY(store.applyDraft(draft, &error));

    QSignalSpy finishedSpy(&store, &SettingsAssignmentStore::applyFinished);
    Q_EMIT transport.commitReceived(transport.commits.first().token,
                                    transport.commits.first().owner,
                                    commitWire(SettingsWireStatus::Applied, 4, 4, current,
                                               QStringList{}));
    QTRY_VERIFY(!finishedSpy.isEmpty());
    const auto outcome = finishedSpy.first().first().value<AssignmentApplyOutcome>();
    QCOMPARE(outcome.status, ApplyStatus::AppliedNoOp);
}

void AssignmentStoreTests::conflictIsReportedAndNeverReplayed()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);
    bringReady(transport, client, assignmentValue(QStringLiteral("p"), QString()));

    ColorAssignmentDraft draft;
    draft.entries.append({QStringLiteral("DP-1"), QStringLiteral("p2"), QByteArray(), false});
    QString error;
    QVERIFY(store.applyDraft(draft, &error));

    QSignalSpy finishedSpy(&store, &SettingsAssignmentStore::applyFinished);
    const QVariant current = assignmentValue(QStringLiteral("p"), QString());
    Q_EMIT transport.commitReceived(transport.commits.first().token,
                                    transport.commits.first().owner,
                                    commitWire(SettingsWireStatus::Conflict, 5, 5, current,
                                               QStringList{}));
    QTRY_VERIFY(!finishedSpy.isEmpty());
    const auto outcome = finishedSpy.first().first().value<AssignmentApplyOutcome>();
    QCOMPARE(outcome.status, ApplyStatus::Conflict);
    QCOMPARE(outcome.reasonCode, QStringLiteral("revision-conflict"));

    // No optimistic retry exists: the transport saw exactly one commit.
    QTest::qWait(80);
    QCOMPARE(transport.commits.size(), 1);
    QCOMPARE(store.writeInFlight(), false);
}

void AssignmentStoreTests::uncertainTimeoutIsTerminalWithoutReplay()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);
    bringReady(transport, client, assignmentValue(QStringLiteral("p"), QString()));

    ColorAssignmentDraft draft;
    draft.entries.append({QStringLiteral("DP-1"), QStringLiteral("p2"), QByteArray(), false});
    QString error;
    QVERIFY(store.applyDraft(draft, &error));

    QSignalSpy finishedSpy(&store, &SettingsAssignmentStore::applyFinished);
    // The reply never arrives; the client times the write out as uncertain.
    QTRY_VERIFY(!finishedSpy.isEmpty());
    const auto outcome = finishedSpy.first().first().value<AssignmentApplyOutcome>();
    QCOMPARE(outcome.status, ApplyStatus::Uncertain);
    QCOMPARE(store.writeInFlight(), false);

    // Transport loss never becomes a replay: no second commit may appear.
    QTest::qWait(150);
    QCOMPARE(transport.commits.size(), 1);

    // After resync to fresh authority an explicit new apply works.
    bringReady(transport, client, assignmentValue(QStringLiteral("p"), QString()));
    QVERIFY(store.applyDraft(draft, &error));
    QCOMPARE(transport.commits.size(), 2);
}

void AssignmentStoreTests::hostilePersistedDocumentFailsClosed()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);

    Q_EMIT transport.ownerChanged(QStringLiteral(":1.42"));
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto request = transport.snapshots.takeFirst();
    // A persisted value outside the documented record grammar.
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner,
        snapshotWire(QStringLiteral("epoch-a"), 4,
                     QVariantMap{{QStringLiteral("DP-1"), QVariantMap{
                                       {QStringLiteral("profile"), QStringLiteral("p")},
                                       {QStringLiteral("lineage"), QStringLiteral("zz")},
                                       {QStringLiteral("extra"), true}}}}));
    QTRY_VERIFY(client.state() == ClientState::Ready);

    const AssignmentDocumentView view = store.document();
    QCOMPARE(view.availability, DocumentAvailability::UnusableDocument);
    QVERIFY(view.reasonCode.startsWith(QStringLiteral("document-unusable/")));

    ColorAssignmentDraft draft;
    draft.entries.append({QStringLiteral("DP-1"), QStringLiteral("p"), QByteArray(), false});
    QString error;
    QVERIFY(!store.applyDraft(draft, &error));
    QCOMPARE(error, QStringLiteral("document-unusable/record-field-set"));
    QVERIFY(transport.commits.isEmpty());
}

void AssignmentStoreTests::documentBecomesUnavailableOnTransportLoss()
{
    FakeTransport transport;
    SettingsClient client(transport, {QLatin1String(kKey)}, testTiming());
    QVERIFY(client.start());
    SettingsAssignmentStore store(client);
    bringReady(transport, client, assignmentValue(QStringLiteral("p"), QString()));

    Q_EMIT transport.ownerChanged(QString());
    QTRY_VERIFY(client.state() == ClientState::Unavailable);
    // The last confirmed document is retained but never reported as live
    // authority: availability is Unavailable and writes are refused.
    QCOMPARE(store.document().availability, DocumentAvailability::Unavailable);
    QVERIFY(store.document().document.records.size() == 1);

    ColorAssignmentDraft draft;
    draft.entries.append({QStringLiteral("DP-1"), QStringLiteral("p"), QByteArray(), false});
    QString error;
    QVERIFY(!store.applyDraft(draft, &error));
    QCOMPARE(error, QStringLiteral("unavailable"));
}

QTEST_MAIN(AssignmentStoreTests)
#include "tst_assignment_store.moc"
