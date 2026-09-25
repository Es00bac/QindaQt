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
  // ADR-0270: preferences-v2 and per-folder views.
  void persistsTheDetailsPresentation();
  void remembersAFolderViewOnlyWhenItChanges();
  void useAsDefaultsPromotesAFolderView();
  void refusesAMalformedFolderView();
  // ADR-0271: the File manager style.
  void pickingAStyleAppliesItsPreset();
  void followsTheLayoutUntilAStyleIsPicked();
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

  controller->setDefaultViewMode(QStringLiteral("carousel"));
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

namespace {

[[nodiscard]] QVariantMap viewMap(const QString &mode, int iconSize,
                                  const QVariantList &columns) {
  return {{QStringLiteral("viewMode"), mode},
          {QStringLiteral("sortColumn"), QStringLiteral("size")},
          {QStringLiteral("sortDirection"), QStringLiteral("descending")},
          {QStringLiteral("groupBy"), QStringLiteral("kind")},
          {QStringLiteral("iconSize"), iconSize},
          {QStringLiteral("columns"), columns}};
}

[[nodiscard]] QVariantList nameAndSize(int sizeWidth) {
  return {QVariantMap{{QStringLiteral("key"), QStringLiteral("name")}, {QStringLiteral("width"), 0}},
          QVariantMap{{QStringLiteral("key"), QStringLiteral("size")},
                      {QStringLiteral("width"), sizeWidth}}};
}

} // namespace

void TestPreferencesController::persistsTheDetailsPresentation() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  auto controller = controllerIn(directory);
  QCOMPARE(controller->groupKeys(), Preferences::groupKeys());
  QCOMPARE(controller->columnKeys(), Preferences::columnKeys());

  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);
  controller->setGroupBy(QStringLiteral("date"));
  controller->setDetailsColumns(nameAndSize(120));
  controller->setRelativeDates(true);
  controller->setRowDensity(QStringLiteral("compact"));
  controller->setShowExtensions(false);
  QCOMPARE(changed.count(), 5);

  const auto reopened = controllerIn(directory);
  QCOMPARE(reopened->groupBy(), QStringLiteral("date"));
  QCOMPARE(reopened->relativeDates(), true);
  QCOMPARE(reopened->rowDensity(), QStringLiteral("compact"));
  QCOMPARE(reopened->showExtensions(), false);
  const QVariantList columns = reopened->detailsColumns();
  QCOMPARE(columns.size(), 2);
  QCOMPARE(columns.at(1).toMap().value(QStringLiteral("width")).toInt(), 120);
}

void TestPreferencesController::remembersAFolderViewOnlyWhenItChanges() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  auto controller = controllerIn(directory);
  const QString pictures = QStringLiteral("/home/user/Pictures");
  QVERIFY(!controller->folderView(pictures).value(QStringLiteral("remembered")).toBool());

  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);
  QVERIFY(controller->rememberFolderView(pictures, viewMap(QStringLiteral("gallery"), 128,
                                                           nameAndSize(0))));
  QCOMPARE(changed.count(), 1);
  // The same view again writes nothing: the window asks after every change.
  QVERIFY(controller->rememberFolderView(pictures, viewMap(QStringLiteral("gallery"), 128,
                                                           nameAndSize(0))));
  QCOMPARE(changed.count(), 1);

  const QVariantMap view = controllerIn(directory)->folderView(pictures);
  QVERIFY(view.value(QStringLiteral("remembered")).toBool());
  QCOMPARE(view.value(QStringLiteral("viewMode")).toString(), QStringLiteral("gallery"));
  QCOMPARE(view.value(QStringLiteral("iconSize")).toInt(), 128);
  // A folder the user never touched shows the defaults.
  const QVariantMap other = controller->folderView(QStringLiteral("/home/user/Documents"));
  QCOMPARE(other.value(QStringLiteral("viewMode")).toString(), QStringLiteral("grid"));
  QVERIFY(!other.value(QStringLiteral("remembered")).toBool());
}

void TestPreferencesController::useAsDefaultsPromotesAFolderView() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));
  const QVariantMap details = viewMap(QStringLiteral("list"), 48, nameAndSize(0));
  QVERIFY(controller->rememberFolderView(QStringLiteral("/a"), details));
  QVERIFY(controller->rememberFolderView(QStringLiteral("/b"), details));
  QVERIFY(controller->rememberFolderView(
      QStringLiteral("/c"), viewMap(QStringLiteral("columns"), 48, nameAndSize(0))));

  QVERIFY(controller->useAsDefaults(details));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("list"));
  QCOMPARE(controller->sortColumn(), QStringLiteral("size"));
  QCOMPARE(controller->sortDirection(), QStringLiteral("descending"));
  QCOMPARE(controller->groupBy(), QStringLiteral("kind"));
  QCOMPARE(controller->iconSize(), 48);
  // Folders whose own view now equals the defaults follow them from here on;
  // one with a different view keeps it, and an untouched one gets the new
  // defaults.
  QVERIFY(!controller->folderView(QStringLiteral("/a")).value(QStringLiteral("remembered")).toBool());
  QVERIFY(!controller->folderView(QStringLiteral("/b")).value(QStringLiteral("remembered")).toBool());
  QCOMPARE(controller->folderView(QStringLiteral("/c")).value(QStringLiteral("viewMode")).toString(),
           QStringLiteral("columns"));
  QCOMPARE(controller->folderView(QStringLiteral("/new")).value(QStringLiteral("viewMode")).toString(),
           QStringLiteral("list"));
}

void TestPreferencesController::refusesAMalformedFolderView() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));
  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);
  QVariantMap view = viewMap(QStringLiteral("gallery"), 57, nameAndSize(0));
  QVERIFY(!controller->rememberFolderView(QStringLiteral("/a"), view));
  view = viewMap(QStringLiteral("gallery"), 64, nameAndSize(12));
  QVERIFY(!controller->rememberFolderView(QStringLiteral("/a"), view));
  QCOMPARE(changed.count(), 0);
  QVERIFY(!controller->storeError().isEmpty());
  QVERIFY(!controller->folderView(QStringLiteral("/a")).value(QStringLiteral("remembered")).toBool());
}

void TestPreferencesController::pickingAStyleAppliesItsPreset() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  auto controller = controllerIn(directory);
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("finder"));
  QVERIFY(controller->fileManagerStyleChoice().isEmpty());
  QCOMPARE(controller->fileManagerStyles(),
           QStringList({QStringLiteral("finder"), QStringLiteral("explorer"),
                        QStringLiteral("commander")}));

  // One write: the style and its starting view.
  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);
  controller->setFileManagerStyle(QStringLiteral("explorer"));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("explorer"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("list"));
  controller->setFileManagerStyle(QStringLiteral("commander"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("list"));
  auto reopened = controllerIn(directory);
  QCOMPARE(reopened->fileManagerStyleChoice(), QStringLiteral("commander"));
  QCOMPARE(reopened->defaultViewMode(), QStringLiteral("list"));

  // "Open folders as" stays the user's own after the preset.
  controller->setDefaultViewMode(QStringLiteral("columns"));
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("commander"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("columns"));
  controller->setFileManagerStyle(QStringLiteral("finder"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("grid"));

  // A style outside the set is refused whole, preset included.
  changed.clear();
  controller->setFileManagerStyle(QStringLiteral("norton"));
  QCOMPARE(changed.count(), 0);
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("finder"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("grid"));
  QVERIFY(!controller->storeError().isEmpty());
}

void TestPreferencesController::followsTheLayoutUntilAStyleIsPicked() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  auto controller = controllerIn(directory);
  QSignalSpy changed(controller.get(), &PreferencesController::preferencesChanged);

  // A Windows-like layout: Explorer, and its preset written once.
  controller->setLayoutStyleHint(QStringLiteral("explorer"));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(controller->layoutStyleHint(), QStringLiteral("explorer"));
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("explorer"));
  QVERIFY(controller->fileManagerStyleChoice().isEmpty());
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("list"));
  QCOMPARE(controller->values().layoutStyle, QStringLiteral("explorer"));

  // A later "Open folders as" sticks, here and on the next launch under the
  // same layout.
  controller->setDefaultViewMode(QStringLiteral("grid"));
  auto reopened = controllerIn(directory);
  reopened->setLayoutStyleHint(QStringLiteral("explorer"));
  QCOMPARE(reopened->fileManagerStyle(), QStringLiteral("explorer"));
  QCOMPARE(reopened->defaultViewMode(), QStringLiteral("grid"));
  reopened.reset();

  // Another layout brings its own style and preset.
  controller->setLayoutStyleHint(QStringLiteral("commander"));
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("commander"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("list"));
  // An unknown hint, or none, is Finder.
  controller->setLayoutStyleHint(QStringLiteral("nautilus"));
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("finder"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("grid"));

  // A pick outlasts every layout change...
  controller->setLayoutStyleHint(QStringLiteral("explorer"));
  controller->setFileManagerStyle(QStringLiteral("finder"));
  controller->setLayoutStyleHint(QStringLiteral("commander"));
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("finder"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("grid"));
  // ...until "Match the desktop layout" is chosen again, which applies the
  // layout's preset at once.
  controller->setFileManagerStyle(QString());
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("commander"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("list"));

  // Restore Defaults matches the layout, preset applied.
  controller->setDefaultViewMode(QStringLiteral("gallery"));
  controller->restoreDefaults();
  QVERIFY(controller->fileManagerStyleChoice().isEmpty());
  QCOMPARE(controller->fileManagerStyle(), QStringLiteral("commander"));
  QCOMPARE(controller->defaultViewMode(), QStringLiteral("list"));
}

QTEST_MAIN(TestPreferencesController)
#include "tst_preferences_controller.moc"
