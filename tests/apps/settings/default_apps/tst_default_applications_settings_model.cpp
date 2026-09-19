// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_settings_model.h>

#include <qindaqt/application_catalog/application_directory_scan.h>

#include <QtTest>

using QindaQt::ApplicationCatalog::DirectoryScan;
using QindaQt::ApplicationCatalog::ScannedApplication;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationCategory;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationPreferences;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationsSettingsModel;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationsStore;

namespace {

class StubStore final : public DefaultApplicationsStore {
public:
  DefaultApplicationPreferences preferences;
  bool loadFails = false;
  bool saveFails = false;
  int saveCalls = 0;
  DefaultApplicationCategory lastCategory{};
  QString lastDesktopId;

  bool load(DefaultApplicationPreferences *out, QString *error) override {
    if (loadFails) {
      if (error) *error = QStringLiteral("stub-load-failed");
      return false;
    }
    *out = preferences;
    return true;
  }
  bool saveCategory(DefaultApplicationCategory category, const QString &desktopId,
                    QString *error) override {
    ++saveCalls;
    lastCategory = category;
    lastDesktopId = desktopId;
    if (saveFails) {
      if (error) *error = QStringLiteral("stub-save-failed");
      return false;
    }
    preferences.setCategory(category, desktopId);
    return true;
  }
};

DirectoryScan makeScan() {
  DirectoryScan scan;
  ScannedApplication browser;
  browser.entry.id = QStringLiteral("browser");
  browser.entry.name = QStringLiteral("Test Browser");
  browser.documentText = QStringLiteral("[Desktop Entry]\nMimeType=text/html;\n");
  scan.applications = {browser};
  return scan;
}

} // namespace

class DefaultApplicationsSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rowsReflectStoredPreferencesAndCandidates();
  void setDefaultApplicationPersistsAndUpdatesRows();
  void setDefaultApplicationOnUnknownCategoryFailsWithoutSaving();
  void loadFailureIsReportedAndRetryClearsIt();
  void pdfChoiceDoesNotRewriteStaleImagePreference();
  void failedSaveKeepsConfirmedSelection();
};

void DefaultApplicationsSettingsModelTest::rowsReflectStoredPreferencesAndCandidates() {
  auto store = std::make_unique<StubStore>();
  store->preferences.browser = QStringLiteral("browser.desktop");
  DefaultApplicationsSettingsModel model(std::move(store), makeScan());

  const QVariantList rows = model.rows();
  QVERIFY(!rows.isEmpty());
  const QVariantMap browserRow = rows.constFirst().toMap();
  QCOMPARE(browserRow.value(QStringLiteral("id")).toString(),
           QStringLiteral("browser"));
  QCOMPARE(browserRow.value(QStringLiteral("currentId")).toString(),
           QStringLiteral("browser.desktop"));
  QCOMPARE(browserRow.value(QStringLiteral("currentName")).toString(),
           QStringLiteral("Test Browser"));
  const QVariantList options = browserRow.value(QStringLiteral("options")).toList();
  QCOMPARE(options.size(), 1);
}

void DefaultApplicationsSettingsModelTest::
    setDefaultApplicationPersistsAndUpdatesRows() {
  auto storeOwned = std::make_unique<StubStore>();
  StubStore *store = storeOwned.get();
  DefaultApplicationsSettingsModel model(std::move(storeOwned), makeScan());
  QSignalSpy changedSpy(&model, &DefaultApplicationsSettingsModel::changed);

  QVERIFY(model.setDefaultApplication(QStringLiteral("browser"),
                                      QStringLiteral("browser.desktop")));
  QCOMPARE(store->saveCalls, 1);
  QCOMPARE(store->lastCategory, DefaultApplicationCategory::Browser);
  QCOMPARE(store->lastDesktopId, QStringLiteral("browser.desktop"));
  QCOMPARE(changedSpy.size(), 1);

  const QVariantList rows = model.rows();
  const QVariantMap browserRow = rows.constFirst().toMap();
  QCOMPARE(browserRow.value(QStringLiteral("currentId")).toString(),
           QStringLiteral("browser.desktop"));
}

void DefaultApplicationsSettingsModelTest::
    setDefaultApplicationOnUnknownCategoryFailsWithoutSaving() {
  auto storeOwned = std::make_unique<StubStore>();
  StubStore *store = storeOwned.get();
  DefaultApplicationsSettingsModel model(std::move(storeOwned), makeScan());
  QVERIFY(!model.setDefaultApplication(QStringLiteral("not-a-category"),
                                       QStringLiteral("x.desktop")));
  QCOMPARE(store->saveCalls, 0);
  QVERIFY(!model.errorText().isEmpty());
}

void DefaultApplicationsSettingsModelTest::
    loadFailureIsReportedAndRetryClearsIt() {
  auto storeOwned = std::make_unique<StubStore>();
  StubStore *store = storeOwned.get();
  store->loadFails = true;
  DefaultApplicationsSettingsModel model(std::move(storeOwned), makeScan());
  QVERIFY(model.loadFailed());
  QVERIFY(!model.errorText().isEmpty());

  store->loadFails = false;
  QVERIFY(model.retry());
  QVERIFY(!model.loadFailed());
  QVERIFY(model.errorText().isEmpty());
}

void DefaultApplicationsSettingsModelTest::pdfChoiceDoesNotRewriteStaleImagePreference() {
  auto storeOwned = std::make_unique<StubStore>();
  StubStore *store = storeOwned.get();
  store->preferences.imageViewer = QStringLiteral("first-image.desktop");
  DefaultApplicationsSettingsModel model(std::move(storeOwned), makeScan());
  // Another preferences tool edits images after Settings has loaded.
  store->preferences.imageViewer = QStringLiteral("new-image.desktop");
  QVERIFY(model.setDefaultApplication(QStringLiteral("pdf-viewer"),
                                     QStringLiteral("pdf.desktop")));
  QCOMPARE(store->lastCategory, DefaultApplicationCategory::PdfViewer);
  QCOMPARE(store->preferences.pdfViewer, QStringLiteral("pdf.desktop"));
  QCOMPARE(store->preferences.imageViewer, QStringLiteral("new-image.desktop"));
  const auto rows = model.rows();
  QCOMPARE(rows.size(), 8);
  for (const auto &row : rows) {
    if (row.toMap().value(QStringLiteral("id")) == QStringLiteral("image-viewer"))
      QCOMPARE(row.toMap().value(QStringLiteral("currentId")).toString(),
               QStringLiteral("new-image.desktop"));
  }
}

void DefaultApplicationsSettingsModelTest::failedSaveKeepsConfirmedSelection() {
  auto storeOwned = std::make_unique<StubStore>();
  StubStore *store = storeOwned.get();
  store->preferences.browser = QStringLiteral("browser.desktop");
  store->saveFails = true;
  DefaultApplicationsSettingsModel model(std::move(storeOwned), makeScan());
  QVERIFY(!model.setDefaultApplication(QStringLiteral("browser"), {}));
  QCOMPARE(model.rows().first().toMap().value(QStringLiteral("currentId")).toString(),
           QStringLiteral("browser.desktop"));
  QCOMPARE(model.errorText(), QStringLiteral("stub-save-failed"));
}

QTEST_GUILESS_MAIN(DefaultApplicationsSettingsModelTest)
#include "tst_default_applications_settings_model.moc"
