// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/preferences_controller.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] std::unique_ptr<PreferencesController> controllerIn(
    const QString &directory) {
  return std::make_unique<PreferencesController>(
      std::make_unique<PreferencesStore>(directory));
}

} // namespace

class TestPreferencesController final : public QObject {
  Q_OBJECT

private slots:
  void startsAtTheDefaultsAndPersistsAChange();
  void publishesTheAcceptedValueSetsForThePickers();
  void refusesAValueOutsideItsSetWithoutChangingAnything();
  void restoreDefaultsReturnsEverything();
  void aRefusedWriteLeavesThePublishedValuesAlone();
};

void TestPreferencesController::startsAtTheDefaultsAndPersistsAChange() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));

  auto controller = controllerIn(directory);
  QCOMPARE(controller->values(), Preferences{});
  QVERIFY(controller->storeError().isEmpty());

  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);
  controller->setShowHidden(true);
  controller->setDefaultViewMode(QStringLiteral("list"));
  controller->setIconSize(128);
  controller->setDiscoverNearbyServers(true);
  controller->setConfirmTrash(false);
  controller->setSortColumn(QStringLiteral("modified"));
  controller->setSortDirection(QStringLiteral("descending"));
  controller->setDirectoriesFirst(false);
  controller->setDefaultConnectScheme(QStringLiteral("smb"));
  QCOMPARE(changed.count(), 9);

  // Setting the same value again writes nothing and says nothing.
  controller->setShowHidden(true);
  QCOMPARE(changed.count(), 9);

  // The next launch reads it all back.
  const auto reopened = controllerIn(directory);
  QCOMPARE(reopened->values(), controller->values());
  QCOMPARE(reopened->showHidden(), true);
  QCOMPARE(reopened->iconSize(), 128);
  QCOMPARE(reopened->confirmTrash(), false);
  QCOMPARE(reopened->defaultConnectScheme(), QStringLiteral("smb"));
}

void TestPreferencesController::publishesTheAcceptedValueSetsForThePickers() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));
  // The window's pickers read these, so they cannot drift from the schema.
  QCOMPARE(controller->viewModes(), Preferences::viewModes());
  QCOMPARE(controller->sortColumns(), Preferences::sortColumns());
  QCOMPARE(controller->sortDirections(), Preferences::sortDirections());
  QCOMPARE(controller->iconSizes().size(), Preferences::iconSizes().size());
  QCOMPARE(controller->iconSizes().constFirst().toInt(),
           Preferences::iconSizes().constFirst());
  QCOMPARE(controller->schemes(),
           QStringList({QStringLiteral("sftp"), QStringLiteral("smb")}));
}

void TestPreferencesController::refusesAValueOutsideItsSetWithoutChangingAnything() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));
  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);

  controller->setDefaultViewMode(QStringLiteral("columns"));
  controller->setSortColumn(QStringLiteral("owner"));
  controller->setIconSize(57);
  controller->setDefaultConnectScheme(QStringLiteral("ftp"));
  QCOMPARE(changed.count(), 0);
  QCOMPARE(controller->values(), Preferences{});
  QVERIFY(!controller->storeError().isEmpty());
  controller->clearStoreError();
  QVERIFY(controller->storeError().isEmpty());
}

void TestPreferencesController::restoreDefaultsReturnsEverything() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  auto controller = controllerIn(directory);
  controller->setShowHidden(true);
  controller->setIconSize(16);
  QVERIFY(controller->values() != Preferences{});

  controller->restoreDefaults();
  QCOMPARE(controller->values(), Preferences{});
  QCOMPARE(controllerIn(directory)->values(), Preferences{});
}

void TestPreferencesController::aRefusedWriteLeavesThePublishedValuesAlone() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  // AGENT-GUARD: a state root the store refuses (a symlinked ancestor) must
  // leave the window showing what the next launch will actually read.
  const QString real = temporary.filePath(QStringLiteral("real"));
  QVERIFY(QDir().mkpath(real));
  const QString linked = temporary.filePath(QStringLiteral("linked"));
  QVERIFY(QFile::link(real, linked));

  auto controller = controllerIn(linked);
  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);
  controller->setShowHidden(true);
  QCOMPARE(changed.count(), 0);
  QCOMPARE(controller->showHidden(), false);
  QVERIFY(!controller->storeError().isEmpty());
}

QTEST_MAIN(TestPreferencesController)
#include "tst_preferences_controller.moc"
