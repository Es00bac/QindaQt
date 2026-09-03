// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcher_persistence.h"
#include "launcher_runtime_test_support.h"

#include <qindaqt/services/settings_client/settings_client.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;
using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Tests::Launcher;

namespace {

QVariantList idList(const QStringList &ids)
{
    QVariantList values;
    for (const QString &id : ids)
        values.append(id);
    return values;
}

const QString kEpoch = QStringLiteral("epoch-a");

struct WiredController {
    FakeSettingsTransport transport;
    SettingsClient client;
    LauncherPersistenceController controller;

    WiredController()
        : client(transport,
                 { LauncherPersistenceController::pinnedKey(),
                   LauncherPersistenceController::recentKey() },
                 ClientTiming { .requestTimeoutMilliseconds = 500,
                                .debounceMilliseconds = 1,
                                .retryMilliseconds = { 10 } })
        , controller(client)
    {
    }

    void publishBaseline(const QVariantMap &values = {}, quint64 revision = 0)
    {
        QVERIFY(client.start());
        transport.announceOwner();
        QTRY_VERIFY(!transport.snapshots.isEmpty());
        transport.replyLastSnapshot(
            FakeSettingsTransport::snapshotWire(kEpoch, revision, values));
        QTRY_VERIFY(controller.persistenceReady());
    }

    // The client re-reads authority after every commit outcome (its state is
    // Authenticating until the resync snapshot lands), so a settled commit is
    // a commit reply plus its follow-up snapshot reply.
    void settle(const QVariantMap &commitReply, const QVariantMap &values,
                quint64 revision)
    {
        const qsizetype snapshotsBefore = transport.snapshots.size();
        transport.replyLastCommit(commitReply);
        QTRY_COMPARE(transport.snapshots.size(), snapshotsBefore + 1);
        transport.replyLastSnapshot(
            FakeSettingsTransport::snapshotWire(kEpoch, revision, values));
        QTRY_VERIFY(controller.persistenceReady());
        QVERIFY(!controller.writeInFlight());
    }

    void settleApplied(const QVariantMap &values, quint64 baseRevision)
    {
        settle(FakeSettingsTransport::commitWire(SettingsWireStatus::Applied,
                                                 kEpoch, baseRevision,
                                                 baseRevision + 1, values),
               values, baseRevision + 1);
    }

    // A new external baseline arrives as an invalidation hint plus snapshot.
    void publishExternal(const QVariantMap &values, quint64 revision)
    {
        const qsizetype snapshotsBefore = transport.snapshots.size();
        Q_EMIT transport.settingsChanged(QStringLiteral(":1.99"), kEpoch, revision,
                                         { LauncherPersistenceController::pinnedKey() });
        QTRY_COMPARE(transport.snapshots.size(), snapshotsBefore + 1);
        transport.replyLastSnapshot(
            FakeSettingsTransport::snapshotWire(kEpoch, revision, values));
    }
};

} // namespace

class LauncherPersistenceTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pinRoundTripsThroughTheClient();
    void restoresConfirmedListsFromSnapshots();
    void hostileStoredValuesAreIgnored();
    void conflictRevertsToTheConfirmedValue();
    void unknownSchemaKeyFailsClosed();
    void uncertainCommitsAreNeverReplayed();
    void transportLossClearsTruthAndRefusesNewWrites();
    void writesAreSerialized();
    void recentListStaysBounded();
};

void LauncherPersistenceTests::pinRoundTripsThroughTheClient()
{
    WiredController wired;
    wired.publishBaseline();

    QCOMPARE(wired.controller.pin(QStringLiteral("org.qindaqt.editor")),
             PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    const auto &commit = wired.transport.commits.constLast();
    QCOMPARE(commit.operations.size(), 1);
    const auto operation = commit.operations.constFirst().toMap();
    QCOMPARE(operation.value(QStringLiteral("key")).toString(),
             LauncherPersistenceController::pinnedKey());
    QCOMPARE(operation.value(QStringLiteral("value")).toStringList(),
             QStringList({ QStringLiteral("org.qindaqt.editor") }));

    wired.settleApplied(
        {{ LauncherPersistenceController::pinnedKey(),
           idList({ QStringLiteral("org.qindaqt.editor") }) }},
        0);
    QVERIFY(wired.controller.statusText().isEmpty());
    QVERIFY(wired.controller.pinned().contains(QStringLiteral("org.qindaqt.editor")));

    // A second writer's invalidation replaces the model wholesale.
    const QVariantMap external {
        { LauncherPersistenceController::pinnedKey(),
          idList({ QStringLiteral("org.other.app"),
                   QStringLiteral("org.qindaqt.editor") }) },
    };
    wired.publishExternal(external, 2);
    QTRY_COMPARE(wired.controller.pinned().ids().constFirst(),
                 QStringLiteral("org.other.app"));
}

void LauncherPersistenceTests::restoresConfirmedListsFromSnapshots()
{
    WiredController wired;
    wired.publishBaseline(
        {{ LauncherPersistenceController::pinnedKey(),
           idList({ QStringLiteral("a.app"), QStringLiteral("b.app") }) },
         { LauncherPersistenceController::recentKey(),
           idList({ QStringLiteral("b.app"), QStringLiteral("a.app") }) }});

    QCOMPARE(wired.controller.pinned().ids(),
             QStringList({ QStringLiteral("a.app"), QStringLiteral("b.app") }));
    QCOMPARE(wired.controller.recent().ids(),
             QStringList({ QStringLiteral("b.app"), QStringLiteral("a.app") }));
}

void LauncherPersistenceTests::hostileStoredValuesAreIgnored()
{
    QStringList tooMany;
    for (int index = 0; index < 40; ++index)
        tooMany.append(QStringLiteral("app.%1").arg(index));

    WiredController wired;
    wired.publishBaseline(
        {{ LauncherPersistenceController::pinnedKey(), idList(tooMany) },
         { LauncherPersistenceController::recentKey(),
           QVariantList { 42, QStringLiteral("valid.app") } }});

    // The oversized pinned list and the non-string recent list are rejected
    // wholesale; nothing partial enters the models.
    QVERIFY(wired.controller.pinned().ids().isEmpty());
    QVERIFY(wired.controller.recent().ids().isEmpty());
    QVERIFY(!wired.controller.statusText().isEmpty());
    // The launcher remains usable; only persistence truth is degraded.
    QVERIFY(wired.controller.persistenceReady());
}

void LauncherPersistenceTests::conflictRevertsToTheConfirmedValue()
{
    WiredController wired;
    wired.publishBaseline(
        {{ LauncherPersistenceController::pinnedKey(),
           idList({ QStringLiteral("confirmed.app") }) }},
        7);

    QCOMPARE(wired.controller.pin(QStringLiteral("new.app")),
             PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    QVERIFY(wired.controller.pinned().contains(QStringLiteral("new.app")));

    // Another writer moved the repository to revision 8; the client's base
    // was 7, so the commit conflicts and the resync restores authority.
    wired.settle(FakeSettingsTransport::commitWire(
                     SettingsWireStatus::Conflict, kEpoch, 8, 8,
                     {{ LauncherPersistenceController::pinnedKey(),
                        idList({ QStringLiteral("confirmed.app") }) }}),
                 {{ LauncherPersistenceController::pinnedKey(),
                    idList({ QStringLiteral("confirmed.app") }) }},
                 8);
    // AGENT-GUARD: A confirmed rejection never leaves the model claiming a
    // save that did not happen (ADR-0012).
    QCOMPARE(wired.controller.pinned().ids(),
             QStringList({ QStringLiteral("confirmed.app") }));
    QVERIFY(!wired.controller.statusText().isEmpty());
}

void LauncherPersistenceTests::unknownSchemaKeyFailsClosed()
{
    // The launcher key set is documented in ADR-0062; until the Settings1
    // schema registers it, the production service answers UnknownKey. The
    // controller must fail closed and say so, not pretend the save landed.
    WiredController wired;
    wired.publishBaseline();

    QCOMPARE(wired.controller.pin(QStringLiteral("org.qindaqt.editor")),
             PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    // UnknownKey keeps revisions unchanged and carries exactly empty
    // value/source authority maps (Settings1 protocol).
    wired.settle(FakeSettingsTransport::commitWire(SettingsWireStatus::UnknownKey,
                                                   kEpoch, 0, 0, {}),
                 {}, 0);
    QVERIFY(wired.controller.pinned().ids().isEmpty());
    QVERIFY(!wired.controller.statusText().isEmpty());
    QVERIFY(wired.controller.persistenceReady());
}

void LauncherPersistenceTests::uncertainCommitsAreNeverReplayed()
{
    WiredController wired;
    wired.publishBaseline();

    QCOMPARE(wired.controller.pin(QStringLiteral("org.qindaqt.editor")),
             PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    QCOMPARE(wired.transport.commits.size(), 1);

    const auto commit = wired.transport.commits.constLast();
    Q_EMIT wired.transport.requestFailed(
        commit.token, commit.owner,
        QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
        QStringLiteral("timed out"));
    QTRY_VERIFY(!wired.controller.writeInFlight());
    QVERIFY(!wired.controller.statusText().isEmpty());

    // No automatic replay, ever: the resync snapshot is the only recovery.
    // The authority deliberately remains byte-for-byte equal to the baseline;
    // this is the former regression where confirmed-vs-confirmed comparison
    // left the optimistic live mutation behind.
    QTest::qWait(50);
    QCOMPARE(wired.transport.commits.size(), 1);
    const qsizetype snapshotsBefore = wired.transport.snapshots.size();
    QTRY_COMPARE(wired.transport.snapshots.size(), snapshotsBefore + 1);
    wired.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        kEpoch, 0, {}));
    QTRY_VERIFY(wired.controller.persistenceReady());
    QTRY_VERIFY(wired.controller.pinned().ids().isEmpty());
    QCOMPARE(wired.transport.commits.size(), 1);
}

void LauncherPersistenceTests::transportLossClearsTruthAndRefusesNewWrites()
{
    WiredController wired;
    wired.publishBaseline(
        {{ LauncherPersistenceController::pinnedKey(),
           idList({ QStringLiteral("kept.app") }) }});

    Q_EMIT wired.transport.busDisconnected();
    QTRY_VERIFY(!wired.controller.persistenceReady());
    // Settings identities are owner-bound: loss clears prior truth, and new
    // writes remain unavailable until a replacement publishes a baseline.
    QVERIFY(wired.controller.pinned().ids().isEmpty());
    QCOMPARE(wired.controller.pin(QStringLiteral("new.app")),
             PersistenceMutation::Unavailable);
    QVERIFY(wired.transport.commits.isEmpty());
}

void LauncherPersistenceTests::writesAreSerialized()
{
    WiredController wired;
    wired.publishBaseline();

    QCOMPARE(wired.controller.pin(QStringLiteral("one.app")),
             PersistenceMutation::Applied);
    QCOMPARE(wired.controller.pin(QStringLiteral("two.app")),
             PersistenceMutation::Busy);
    // The refused mutation did not touch the live model.
    QVERIFY(!wired.controller.pinned().contains(QStringLiteral("two.app")));
}

void LauncherPersistenceTests::recentListStaysBounded()
{
    WiredController wired;
    wired.publishBaseline();

    quint64 revision = 0;
    for (int index = 0; index < 12; ++index) {
        const QString id = QStringLiteral("app.%1").arg(index);
        QCOMPARE(wired.controller.recordLaunch(id), PersistenceMutation::Applied);
        QTRY_VERIFY(!wired.transport.commits.isEmpty());
        const QStringList ids = wired.controller.recent().ids();
        wired.settleApplied(
            {{ LauncherPersistenceController::recentKey(), idList(ids) }},
            revision);
        ++revision;
    }
    QCOMPARE(wired.controller.recent().ids().size(), 8);
    QCOMPARE(wired.controller.recent().ids().constFirst(), QStringLiteral("app.11"));

    QCOMPARE(wired.controller.clearRecent(), PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    // An Applied commit reports the operated key's authoritative value, here
    // the empty list after the clear.
    wired.settle(FakeSettingsTransport::commitWire(
                     SettingsWireStatus::Applied, kEpoch, revision, revision + 1,
                     {{ LauncherPersistenceController::recentKey(), idList({}) }}),
                 {}, revision + 1);
    QVERIFY(wired.controller.recent().ids().isEmpty());
}

QTEST_GUILESS_MAIN(LauncherPersistenceTests)
#include "tst_launcher_persistence.moc"
