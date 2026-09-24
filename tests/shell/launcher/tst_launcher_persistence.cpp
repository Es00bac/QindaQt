// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcher_persistence.h"
#include "launcher_runtime_test_support.h"

#include <qindaqt/services/dock_items/dock_items.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;
using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Tests::Launcher;
using QindaQt::Services::DockItems::DockEditError;
using QindaQt::Services::DockItems::DockItem;
using QindaQt::Services::DockItems::DockItems;

namespace {

QVariantList idList(const QStringList &ids)
{
    QVariantList values;
    for (const QString &id : ids)
        values.append(id);
    return values;
}

// ADR-0265: the stored dock value for a dock of plain applications.
QVariant dockValue(const QStringList &ids)
{
    DockItems dock;
    for (const QString &id : ids)
        (void)dock.insert(dock.size(), DockItem::application(id));
    return DockItems::encodeSettingsValue(dock);
}

QStringList committedDockIds(const QVariant &value)
{
    const auto decoded = DockItems::decodeSettingsValue(value);
    return decoded.ok() && !decoded.unmigrated ? decoded.items->applicationIds()
                                               : QStringList{};
}

const QString kEpoch = QStringLiteral("epoch-a");

struct WiredController {
    FakeSettingsTransport transport;
    SettingsClient client;
    LauncherPersistenceController controller;

    WiredController()
        : client(transport, LauncherPersistenceController::scopedKeys(),
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

    // The one committed operation's value, which must be for `key`.
    QVariant committedValue(const QString &key)
    {
        const auto operation = transport.commits.constLast().operations.constFirst().toMap();
        if (operation.value(QStringLiteral("key")).toString() != key)
            return {};
        return operation.value(QStringLiteral("value"));
    }

    // A new external baseline arrives as an invalidation hint plus snapshot.
    void publishExternal(const QVariantMap &values, quint64 revision)
    {
        const qsizetype snapshotsBefore = transport.snapshots.size();
        Q_EMIT transport.settingsChanged(QStringLiteral(":1.99"), kEpoch, revision,
                                         values.keys());
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
    void availabilityNoticeClearsAfterConfirmedRecovery();
    void pinRoundTripsThroughTheClient();
    void legacyPinsMigrateOnTheFirstDockWrite();
    void malformedDockIsIgnoredUntilTheNextEdit();
    void dockEditsCommitTheWholeDockAndRevertOnRefusal();
    void restoresConfirmedListsFromSnapshots();
    void hostileStoredValuesAreIgnored();
    void conflictRevertsToTheConfirmedValue();
    void unknownSchemaKeyFailsClosed();
    void uncertainCommitsAreNeverReplayed();
    void transportLossClearsTruthAndRefusesNewWrites();
    void writesAreSerialized();
    void recentListStaysBounded();
};

void LauncherPersistenceTests::availabilityNoticeClearsAfterConfirmedRecovery()
{
    WiredController wired;
    QVERIFY(!wired.controller.persistenceReady());
    QVERIFY(!wired.controller.statusText().isEmpty());
    wired.publishBaseline();
    QVERIFY(wired.controller.statusText().isEmpty());

    wired.transport.announceOwner(QString{});
    QVERIFY(!wired.controller.persistenceReady());
    QVERIFY(!wired.controller.statusText().isEmpty());
    const qsizetype before = wired.transport.snapshots.size();
    wired.transport.announceOwner(QStringLiteral(":1.100"));
    QTRY_COMPARE(wired.transport.snapshots.size(), before + 1);
    wired.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("replacement-epoch"), 0, {}));
    QTRY_VERIFY(wired.controller.persistenceReady());
    QVERIFY(wired.controller.statusText().isEmpty());
}

void LauncherPersistenceTests::pinRoundTripsThroughTheClient()
{
    WiredController wired;
    wired.publishBaseline();

    QCOMPARE(wired.controller.pin(QStringLiteral("org.qindaqt.editor")),
             PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    const auto &commit = wired.transport.commits.constLast();
    QCOMPARE(commit.operations.size(), 1);
    // ADR-0265: a pin writes the whole structured dock, never the legacy list.
    const QVariant written =
        wired.committedValue(LauncherPersistenceController::dockItemsKey());
    QCOMPARE(committedDockIds(written),
             QStringList({ QStringLiteral("org.qindaqt.editor") }));

    wired.settleApplied({{ LauncherPersistenceController::dockItemsKey(), written }}, 0);
    QVERIFY(wired.controller.statusText().isEmpty());
    QVERIFY(wired.controller.pinned().contains(QStringLiteral("org.qindaqt.editor")));

    // A second writer's invalidation replaces the model wholesale.
    const QVariantMap external {
        { LauncherPersistenceController::dockItemsKey(),
          dockValue({ QStringLiteral("org.other.app"),
                      QStringLiteral("org.qindaqt.editor") }) },
    };
    wired.publishExternal(external, 2);
    QTRY_COMPARE(wired.controller.pinned().ids().constFirst(),
                 QStringLiteral("org.other.app"));
}

void LauncherPersistenceTests::legacyPinsMigrateOnTheFirstDockWrite()
{
    WiredController wired;
    wired.publishBaseline(
        {{ LauncherPersistenceController::pinnedKey(),
           idList({ QStringLiteral("a.app"), QStringLiteral("b.app") }) }});
    // Unmigrated: the dock is derived from ADR-0076's list.
    QCOMPARE(wired.controller.pinned().ids(),
             QStringList({ QStringLiteral("a.app"), QStringLiteral("b.app") }));
    QCOMPARE(wired.controller.dock().size(), 2);

    QCOMPARE(wired.controller.pin(QStringLiteral("c.app")), PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    // One operation, on the dock key; the legacy list is carried over, not
    // rewritten.
    QCOMPARE(wired.transport.commits.constLast().operations.size(), 1);
    const QVariant written =
        wired.committedValue(LauncherPersistenceController::dockItemsKey());
    QCOMPARE(committedDockIds(written),
             QStringList({ QStringLiteral("a.app"), QStringLiteral("b.app"),
                           QStringLiteral("c.app") }));
    wired.settleApplied(
        {{ LauncherPersistenceController::dockItemsKey(), written },
         { LauncherPersistenceController::pinnedKey(),
           idList({ QStringLiteral("a.app"), QStringLiteral("b.app") }) }},
        0);
    QCOMPARE(wired.controller.pinned().ids().size(), 3);

    // Once migrated, the legacy key is inert: another writer changing it
    // alone changes nothing.
    wired.publishExternal(
        {{ LauncherPersistenceController::dockItemsKey(), written },
         { LauncherPersistenceController::pinnedKey(), idList({ QStringLiteral("z.app") }) }},
        2);
    QTest::qWait(20);
    QCOMPARE(wired.controller.pinned().ids(),
             QStringList({ QStringLiteral("a.app"), QStringLiteral("b.app"),
                           QStringLiteral("c.app") }));
}

void LauncherPersistenceTests::malformedDockIsIgnoredUntilTheNextEdit()
{
    WiredController wired;
    // A future or corrupted dock value is not partially trusted, and the
    // legacy list does not stand in for it.
    wired.publishBaseline(
        {{ LauncherPersistenceController::dockItemsKey(),
           QVariantMap {{ QStringLiteral("version"), qint64(9) },
                        { QStringLiteral("items"), QVariantList {} }} },
         { LauncherPersistenceController::pinnedKey(), idList({ QStringLiteral("a.app") }) }});
    QVERIFY(wired.controller.pinned().ids().isEmpty());
    QVERIFY(wired.controller.dock().isEmpty());
    QVERIFY(!wired.controller.statusText().isEmpty());
    QVERIFY(wired.controller.persistenceReady());

    // The next explicit edit replaces it with a well-formed dock.
    QCOMPARE(wired.controller.pin(QStringLiteral("b.app")), PersistenceMutation::Applied);
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    QCOMPARE(committedDockIds(
                 wired.committedValue(LauncherPersistenceController::dockItemsKey())),
             QStringList({ QStringLiteral("b.app") }));
}

void LauncherPersistenceTests::dockEditsCommitTheWholeDockAndRevertOnRefusal()
{
    WiredController wired;
    wired.publishBaseline(
        {{ LauncherPersistenceController::dockItemsKey(),
           dockValue({ QStringLiteral("writer"), QStringLiteral("calc") }) }},
        3);
    QSignalSpy dockChanged(&wired.controller, &LauncherPersistenceController::dockChanged);

    // Grouping is a dock edit like any other: applied live, committed whole.
    QCOMPARE(wired.controller.editDock([](DockItems &dock) {
                 return dock.combine(0, 1, QStringLiteral("Office"));
             }),
             PersistenceMutation::Applied);
    QVERIFY(dockChanged.size() >= 1);
    QCOMPARE(wired.controller.dock().size(), 1);
    // The pinned projection keeps both members, so the launcher's Pinned
    // section still lists them.
    QCOMPARE(wired.controller.pinned().ids(),
             QStringList({ QStringLiteral("writer"), QStringLiteral("calc") }));
    QTRY_VERIFY(!wired.transport.commits.isEmpty());
    const auto written = DockItems::decodeSettingsValue(
        wired.committedValue(LauncherPersistenceController::dockItemsKey()));
    QVERIFY(written.ok());
    QCOMPARE(written.items->items().constFirst(),
             DockItem::group(QStringLiteral("Office"),
                             { QStringLiteral("writer"), QStringLiteral("calc") }));

    // A refused edit returns the last confirmed dock and keeps the reason.
    wired.settle(FakeSettingsTransport::commitWire(
                     SettingsWireStatus::ValidationFailed, kEpoch, 3, 3,
                     {{ LauncherPersistenceController::dockItemsKey(),
                        dockValue({ QStringLiteral("writer"), QStringLiteral("calc") }) }}),
                 {{ LauncherPersistenceController::dockItemsKey(),
                    dockValue({ QStringLiteral("writer"), QStringLiteral("calc") }) }},
                 3);
    QCOMPARE(wired.controller.dock().size(), 2);
    QVERIFY(!wired.controller.statusText().isEmpty());

    // A model refusal never reaches Settings1 and says why.
    const qsizetype commitsBefore = wired.transport.commits.size();
    QCOMPARE(wired.controller.editDock([](DockItems &dock) {
                 return dock.insert(0, DockItem::application(QStringLiteral("writer")));
             }),
             PersistenceMutation::RejectedByModel);
    QCOMPARE(wired.controller.lastDockEditError(), DockEditError::AlreadyInDock);
    QCOMPARE(wired.transport.commits.size(), commitsBefore);
    QCOMPARE(wired.controller.dock().size(), 2);
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
    // was 7, so the commit conflicts and the resync restores authority. The
    // dock key still holds its schema default (the dock was never written).
    wired.settle(FakeSettingsTransport::commitWire(
                     SettingsWireStatus::Conflict, kEpoch, 8, 8,
                     {{ LauncherPersistenceController::dockItemsKey(), QVariantMap {} }}),
                 {{ LauncherPersistenceController::pinnedKey(),
                    idList({ QStringLiteral("confirmed.app") }) }},
                 8);
    // AGENT-GUARD: A confirmed rejection never leaves the model claiming a
    // save that did not happen (ADR-0012).
    QCOMPARE(wired.controller.pinned().ids(),
             QStringList({ QStringLiteral("confirmed.app") }));
    QVERIFY(!wired.controller.statusText().isEmpty());
    const QString rejectedSave = wired.controller.statusText();
    wired.transport.announceOwner(QString{});
    QCOMPARE(wired.controller.statusText(), rejectedSave);
    const qsizetype before = wired.transport.snapshots.size();
    wired.transport.announceOwner(QStringLiteral(":1.100"));
    QTRY_COMPARE(wired.transport.snapshots.size(), before + 1);
    wired.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("replacement-epoch"), 0,
        {{LauncherPersistenceController::pinnedKey(), idList({QStringLiteral("confirmed.app")})}}));
    QTRY_VERIFY(wired.controller.persistenceReady());
    QCOMPARE(wired.controller.statusText(), rejectedSave);
}

void LauncherPersistenceTests::unknownSchemaKeyFailsClosed()
{
    // An older or incompatible service may reject the registered launcher
    // keys. The controller must report that refusal rather than claim a save.
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
