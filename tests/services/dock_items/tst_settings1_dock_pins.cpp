// SPDX-License-Identifier: LGPL-3.0-or-later
// ADR-0265: the public pin helper other processes use. It writes the whole
// migrated dock, reports Saved only after a same-lineage readback, and never
// replays a refused or uncertain write.
#include <qindaqt/services/dock_items/settings1_dock_pins.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services;
using DockItems::DockItem;
using DockItems::Settings1DockPins;
using SettingsClientType = QindaQt::Services::SettingsClient::SettingsClient;
using SettingsClient::SettingsTransport;
using SettingsProtocol::SettingsWireStatus;
using SettingsProtocol::WireContract;

namespace {

const QString kDockKey = QLatin1StringView(DockItems::DockItemsSettingsKey);
const QString kLegacyKey = QLatin1StringView(DockItems::LegacyPinnedSettingsKey);

class FakeTransport final : public SettingsTransport {
  Q_OBJECT
public:
  struct Request {
    quint64 token;
    QString owner;
    QVariantList operations;
  };
  QList<Request> snapshots;
  QList<Request> commits;
  bool start(QString *error) override
  {
    if (error)
      error->clear();
    return true;
  }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
  {
    snapshots.append({token, owner, {}});
  }
  void commit(quint64 token, const QString &owner, const QString &, quint64,
              const QVariantList &operations) override
  {
    commits.append({token, owner, operations});
  }
  void requestActivation() override {}
};

QVariantMap snapshotWire(quint64 revision, const QVariantMap &values,
                         const QString &epoch = QStringLiteral("epoch"))
{
  QVariantMap sources;
  for (auto it = values.cbegin(); it != values.cend(); ++it)
    sources.insert(it.key(), QStringLiteral("user-overrides"));
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), epoch},
          {QLatin1StringView(WireContract::FieldRevision), revision},
          {QLatin1StringView(WireContract::FieldValues), values},
          {QLatin1StringView(WireContract::FieldSourceLayers), sources},
          {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

// Settings1's reply shape: Applied moves the revision by one; a conflict
// reports the newer revision another writer reached; other refusals keep it.
QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after,
                       const QVariant &dock)
{
  const bool applied = status == SettingsWireStatus::Applied && after == before + 1;
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
          {QLatin1StringView(WireContract::FieldRevisionBefore), before},
          {QLatin1StringView(WireContract::FieldRevisionAfter), after},
          {QLatin1StringView(WireContract::FieldValues), QVariantMap{{kDockKey, dock}}},
          {QLatin1StringView(WireContract::FieldSourceLayers),
           QVariantMap{{kDockKey, QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WireContract::FieldChangedKeys),
           applied ? QStringList{kDockKey} : QStringList{}},
          {QLatin1StringView(WireContract::FieldMessage),
           applied ? QString{} : QStringLiteral("Rejected by Settings1")}};
}

QVariantMap values(const QVariant &dock, const QVariant &legacy = QVariantList{})
{
  return {{kDockKey, dock}, {kLegacyKey, legacy}};
}

struct Harness {
  FakeTransport transport;
  SettingsClientType client{transport, Settings1DockPins::scopedKeys(),
                            {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                             .retryMilliseconds = {10}}};
  Settings1DockPins pins{client};

  void baseline(const QVariantMap &state)
  {
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.40"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner, snapshotWire(1, state));
    QTRY_VERIFY(pins.isLoaded());
  }

  // The one committed operation's dock value.
  QVariant committedDock()
  {
    const auto operation = transport.commits.constLast().operations.constFirst().toMap();
    if (operation.value(QStringLiteral("key")).toString() != kDockKey)
      return {};
    return operation.value(QStringLiteral("value"));
  }

  void answerCommit(SettingsWireStatus status, quint64 before, quint64 after,
                    const QVariant &dock)
  {
    const auto request = transport.commits.takeFirst();
    Q_EMIT transport.commitReceived(request.token, request.owner,
                                    commitWire(status, before, after, dock));
  }

  void answerSnapshot(quint64 revision, const QVariantMap &state)
  {
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(revision, state));
  }
};

} // namespace

class Settings1DockPinsTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void pinningMigratesTheLegacyListAndWaitsForReadback();
  void unpinningRemovesAGroupMember();
  void refusalAndConflictNeverReplay();
  void staleReadbackWaitsThenTimesOutUncertain();
  void malformedStoredDockIsLeftAlone();
  void requestsNeedAConfirmedDockAndOneAtATime();
};

void Settings1DockPinsTests::pinningMigratesTheLegacyListAndWaitsForReadback()
{
  Harness harness;
  harness.baseline(values(QVariantMap{}, QVariantList{QStringLiteral("org.qindaqt.Files")}));
  QVERIFY(harness.pins.isPinned(QStringLiteral("org.qindaqt.Files")));
  QVERIFY(!harness.pins.isPinned(QStringLiteral("org.libreoffice.Writer")));
  QSignalSpy finished(&harness.pins, &Settings1DockPins::requestFinished);

  QVERIFY(harness.pins.pinApplication(QStringLiteral("org.libreoffice.Writer")));
  QVERIFY(harness.pins.writePending());
  QTRY_COMPARE(harness.transport.commits.size(), 1);
  // The whole migrated dock is written: the legacy pin first, then the new.
  const auto written = DockItems::DockItems::decodeSettingsValue(harness.committedDock());
  QVERIFY(written.ok() && !written.unmigrated);
  QCOMPARE(written.items->applicationIds(),
           QStringList({QStringLiteral("org.qindaqt.Files"),
                        QStringLiteral("org.libreoffice.Writer")}));
  const QVariant stored = harness.committedDock();

  harness.answerCommit(SettingsWireStatus::Applied, 1, 2, stored);
  // Applied is not Saved: only the readback snapshot settles it.
  QCOMPARE(finished.size(), 0);
  harness.answerSnapshot(2, values(stored));
  QTRY_COMPARE(finished.size(), 1);
  QCOMPARE(finished.constFirst().at(2).value<Settings1DockPins::Outcome>(),
           Settings1DockPins::Outcome::Saved);
  QVERIFY(!harness.pins.writePending());
  QVERIFY(harness.pins.isPinned(QStringLiteral("org.libreoffice.Writer")));
}

void Settings1DockPinsTests::unpinningRemovesAGroupMember()
{
  DockItems::DockItems dock;
  dock.insert(0, DockItem::group(QStringLiteral("Office"),
                                 {QStringLiteral("writer"), QStringLiteral("calc")}));
  Harness harness;
  harness.baseline(values(DockItems::DockItems::encodeSettingsValue(dock)));
  QVERIFY(harness.pins.isPinned(QStringLiteral("calc")));
  QVERIFY(harness.pins.unpinApplication(QStringLiteral("calc")));
  QTRY_COMPARE(harness.transport.commits.size(), 1);
  const auto written = DockItems::DockItems::decodeSettingsValue(harness.committedDock());
  QCOMPARE(written.items->applicationIds(), QStringList{QStringLiteral("writer")});
  QCOMPARE(written.items->items().constFirst().name, QStringLiteral("Office"));
  QVERIFY(!harness.pins.unpinApplication(QStringLiteral("calc"))); // still pending
}

void Settings1DockPinsTests::refusalAndConflictNeverReplay()
{
  Harness harness;
  harness.baseline(values(QVariantMap{}));
  QSignalSpy finished(&harness.pins, &Settings1DockPins::requestFinished);
  QVERIFY(harness.pins.pinApplication(QStringLiteral("a.app")));
  QTRY_COMPARE(harness.transport.commits.size(), 1);
  harness.answerCommit(SettingsWireStatus::ValidationFailed, 1, 1, QVariantMap{});
  QTRY_COMPARE(finished.size(), 1);
  QCOMPARE(finished.constLast().at(2).value<Settings1DockPins::Outcome>(),
           Settings1DockPins::Outcome::Refused);
  QVERIFY(harness.pins.statusText().contains(QStringLiteral("Rejected")));
  harness.answerSnapshot(1, values(QVariantMap{}));
  QTRY_VERIFY(harness.pins.isLoaded());
  QVERIFY(!harness.pins.isPinned(QStringLiteral("a.app")));

  QVERIFY(harness.pins.pinApplication(QStringLiteral("a.app")));
  QTRY_COMPARE(harness.transport.commits.size(), 1);
  // Another writer moved Settings1 to revision 2 first.
  harness.answerCommit(SettingsWireStatus::Conflict, 2, 2, QVariantMap{});
  QTRY_COMPARE(finished.size(), 2);
  QCOMPARE(finished.constLast().at(2).value<Settings1DockPins::Outcome>(),
           Settings1DockPins::Outcome::Conflict);
  QTest::qWait(50);
  QVERIFY(harness.transport.commits.isEmpty());
}

void Settings1DockPinsTests::staleReadbackWaitsThenTimesOutUncertain()
{
  Harness harness;
  harness.baseline(values(QVariantMap{}));
  QSignalSpy finished(&harness.pins, &Settings1DockPins::requestFinished);
  QVERIFY(harness.pins.pinApplication(QStringLiteral("a.app")));
  QTRY_COMPARE(harness.transport.commits.size(), 1);
  const QVariant stored = harness.committedDock();
  harness.answerCommit(SettingsWireStatus::Applied, 1, 2, stored);
  // A same-lineage snapshot older than the Applied revision cannot settle.
  harness.answerSnapshot(1, values(QVariantMap{}));
  QTest::qWait(50);
  QCOMPARE(finished.size(), 0);
  // The bounded readback never replays the write; it retires as uncertain.
  QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
  QCOMPARE(finished.constFirst().at(2).value<Settings1DockPins::Outcome>(),
           Settings1DockPins::Outcome::Uncertain);
  QVERIFY(harness.transport.commits.isEmpty());
}

void Settings1DockPinsTests::malformedStoredDockIsLeftAlone()
{
  Harness harness;
  harness.baseline(values(QVariantMap{{QStringLiteral("version"), qint64(7)},
                                      {QStringLiteral("items"), QVariantList{}}}));
  QVERIFY(!harness.pins.isPinned(QStringLiteral("a.app")));
  QVERIFY(!harness.pins.pinApplication(QStringLiteral("a.app")));
  QVERIFY(!harness.pins.statusText().isEmpty());
  QTest::qWait(20);
  QVERIFY(harness.transport.commits.isEmpty());
}

void Settings1DockPinsTests::requestsNeedAConfirmedDockAndOneAtATime()
{
  Harness harness;
  QVERIFY(!harness.pins.isLoaded());
  QVERIFY(!harness.pins.pinApplication(QStringLiteral("a.app")));
  harness.baseline(values(QVariantMap{}));
  QVERIFY(!harness.pins.unpinApplication(QStringLiteral("a.app"))); // not pinned
  QVERIFY(harness.pins.pinApplication(QStringLiteral("a.app")));
  QVERIFY(!harness.pins.pinApplication(QStringLiteral("b.app")));
  QVERIFY(!harness.pins.pinApplication(QString{}));
}

QTEST_GUILESS_MAIN(Settings1DockPinsTests)
#include "tst_settings1_dock_pins.moc"
