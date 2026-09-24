// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0273: Keep in Dock for the Applications place over the dock's public pin
// helper (ADR-0265), and the application.keep-in-dock action it drives. A
// scripted Settings1 transport stands in for the service; no bus is reached.
#include "app_shell/file_manager_action_catalog.h"
#include "app_shell/file_manager_dock_actions.h"
#include "runtime/application_dock_pins.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <qindaqt/services/dock_items/dock_items.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <optional>

using namespace QindaQt::Apps::FileManager;
using QindaQt::Services::SettingsClient::SettingsTransport;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

const QString kDockKey = QLatin1StringView(QindaQt::Services::DockItems::DockItemsSettingsKey);
const QString kLegacyKey =
    QLatin1StringView(QindaQt::Services::DockItems::LegacyPinnedSettingsKey);
const QString kKeepInDock = QStringLiteral("application.keep-in-dock");

class ScriptedTransport final : public SettingsTransport {
  Q_OBJECT
public:
  struct Request {
    quint64 token;
    QString owner;
    QVariantList operations;
  };
  QList<Request> snapshots;
  QList<Request> commits;

  bool start(QString *error) override {
    if (error)
      error->clear();
    return true;
  }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override {
    snapshots.append({token, owner, {}});
  }
  void commit(quint64 token, const QString &owner, const QString &, quint64,
              const QVariantList &operations) override {
    commits.append({token, owner, operations});
  }
  void requestActivation() override {}
};

QVariantMap snapshotWire(quint64 revision, const QVariantMap &values) {
  QVariantMap sources;
  for (auto it = values.cbegin(); it != values.cend(); ++it)
    sources.insert(it.key(), QStringLiteral("user-overrides"));
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion),
           WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
          {QLatin1StringView(WireContract::FieldRevision), revision},
          {QLatin1StringView(WireContract::FieldValues), values},
          {QLatin1StringView(WireContract::FieldSourceLayers), sources},
          {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after,
                       const QVariant &dock) {
  const bool applied = status == SettingsWireStatus::Applied && after == before + 1;
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion),
           WireContract::WireSchemaVersion},
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

// The legacy pinned list is what an unmigrated session stores; the helper
// migrates it exactly as the shell's dock does.
QVariantMap legacyDock(const QStringList &pinned) {
  QVariantList legacy;
  for (const QString &id : pinned)
    legacy.append(id);
  return {{kDockKey, QVariantMap{}}, {kLegacyKey, legacy}};
}

struct Harness {
  ScriptedTransport *transport = nullptr;
  std::unique_ptr<ApplicationDockPins> pins;
  QindaQt::AppShell::ApplicationCoordinator coordinator;

  Harness() {
    auto owned = std::make_unique<ScriptedTransport>();
    transport = owned.get();
    pins = std::make_unique<ApplicationDockPins>(std::move(owned));
    const auto replaced = coordinator.replaceActions(fileManagerActionCatalog());
    Q_UNUSED(replaced);
    bindFileManagerDockActions(coordinator, *pins);
  }

  void baseline(const QVariantMap &values) {
    Q_EMIT transport->ownerChanged(QStringLiteral(":1.40"));
    QTRY_COMPARE(transport->snapshots.size(), 1);
    answerSnapshot(1, values);
  }

  void answerSnapshot(quint64 revision, const QVariantMap &values) {
    QTRY_VERIFY(!transport->snapshots.isEmpty());
    const auto request = transport->snapshots.takeFirst();
    Q_EMIT transport->snapshotReceived(request.token, request.owner,
                                       snapshotWire(revision, values));
  }

  QVariant answerCommit(SettingsWireStatus status, quint64 before, quint64 after) {
    const auto request = transport->commits.takeFirst();
    const QVariant dock =
        request.operations.constFirst().toMap().value(QStringLiteral("value"));
    Q_EMIT transport->commitReceived(request.token, request.owner,
                                     commitWire(status, before, after, dock));
    return dock;
  }

  std::optional<QindaQt::AppShell::ActionSpec> action() {
    for (const auto &candidate : coordinator.actionRegistry().actions()) {
      if (candidate.id == kKeepInDock)
        return candidate;
    }
    return std::nullopt;
  }
};

} // namespace

class TestApplicationDockPins final : public QObject {
  Q_OBJECT

private slots:
  void nothingIsAvailableBeforeTheDockOrASelection();
  void keepingInDockWaitsForSettings1ToConfirm();
  void aRefusedWriteLeavesTheCheckUnchanged();
};

void TestApplicationDockPins::nothingIsAvailableBeforeTheDockOrASelection() {
  Harness harness;
  QVERIFY(harness.action().has_value());
  QVERIFY(!harness.action()->enabled);

  // A selection alone does not make Keep in Dock available: no dock is known.
  harness.pins->setApplicationId(QStringLiteral("org.qindaqt.Files"));
  QVERIFY(!harness.pins->available());
  QVERIFY(!harness.pins->toggle());

  harness.baseline(legacyDock({QStringLiteral("org.qindaqt.Files")}));
  QTRY_VERIFY(harness.pins->available());
  QVERIFY(harness.pins->pinned());
  QVERIFY(harness.action()->enabled);
  QVERIFY(harness.action()->checked);

  // Leaving the place (or selecting several rows) clears the application.
  harness.pins->setApplicationId({});
  QVERIFY(!harness.pins->available());
  QVERIFY(!harness.pins->pinned());
  QVERIFY(!harness.action()->enabled);
  QVERIFY(!harness.action()->checked);
  QVERIFY(!harness.pins->toggle());
  QTest::qWait(20);
  QVERIFY(harness.transport->commits.isEmpty());
}

void TestApplicationDockPins::keepingInDockWaitsForSettings1ToConfirm() {
  Harness harness;
  harness.baseline(legacyDock({QStringLiteral("org.qindaqt.Files")}));
  harness.pins->setApplicationId(QStringLiteral("org.libreoffice.Writer"));
  QTRY_VERIFY(harness.pins->available());
  QVERIFY(!harness.pins->pinned());
  QVERIFY(harness.action()->enabled);
  QVERIFY(!harness.action()->checked);

  QVERIFY(harness.pins->toggle());
  // One change at a time, and no check before Settings1 confirms it.
  QVERIFY(!harness.pins->available());
  QVERIFY(!harness.action()->enabled);
  QVERIFY(!harness.action()->checked);
  QTRY_COMPARE(harness.transport->commits.size(), 1);
  const QVariant stored = harness.answerCommit(SettingsWireStatus::Applied, 1, 2);
  QVERIFY(!harness.pins->pinned());
  harness.answerSnapshot(2, {{kDockKey, stored}, {kLegacyKey, QVariantList{}}});
  QTRY_VERIFY(harness.pins->pinned());
  QTRY_VERIFY(harness.pins->available());
  QVERIFY(harness.action()->enabled);
  QVERIFY(harness.action()->checked);

  // The same action removes it again.
  QVERIFY(harness.pins->toggle());
  QTRY_COMPARE(harness.transport->commits.size(), 1);
  const QVariant removed = harness.answerCommit(SettingsWireStatus::Applied, 2, 3);
  harness.answerSnapshot(3, {{kDockKey, removed}, {kLegacyKey, QVariantList{}}});
  QTRY_VERIFY(!harness.pins->pinned());
  QTRY_VERIFY(harness.pins->available());
  QVERIFY(!harness.action()->checked);
}

void TestApplicationDockPins::aRefusedWriteLeavesTheCheckUnchanged() {
  Harness harness;
  harness.baseline(legacyDock({}));
  harness.pins->setApplicationId(QStringLiteral("org.libreoffice.Writer"));
  QTRY_VERIFY(harness.pins->available());
  QVERIFY(harness.pins->toggle());
  QTRY_COMPARE(harness.transport->commits.size(), 1);
  const auto refused = harness.answerCommit(SettingsWireStatus::ValidationFailed, 1, 1);
  Q_UNUSED(refused);
  harness.answerSnapshot(1, legacyDock({}));
  QTRY_VERIFY(harness.pins->available());
  QVERIFY(!harness.pins->pinned());
  QVERIFY(!harness.action()->checked);
  // Nothing is replayed.
  QTest::qWait(50);
  QVERIFY(harness.transport->commits.isEmpty());
}

QTEST_MAIN(TestApplicationDockPins)
#include "tst_application_dock_pins.moc"
