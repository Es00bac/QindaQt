// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_catalog.h>

#include <qindaqt/application_catalog/application_directory_scan.h>

#include <QtTest>

using QindaQt::ApplicationCatalog::DirectoryScan;
using QindaQt::ApplicationCatalog::ScannedApplication;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationCategory;
using QindaQt::Apps::SettingsDefaultApps::candidateApplicationsForCategory;
using QindaQt::Apps::SettingsDefaultApps::desktopEntryMimeTypes;

class DefaultApplicationsCatalogTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void parsesSemicolonSeparatedMimeTypesFromTheDesktopEntryGroupOnly();
  void ignoresAMimeTypeLikeKeyOutsideDesktopEntry();
  void filtersCandidatesByAnyCategoryMimeType();
  void pdfAndImageCategoriesAreIndependent();
  void effectiveAssociationProjectionControlsCandidatesAndScope();
};

void DefaultApplicationsCatalogTest::
    parsesSemicolonSeparatedMimeTypesFromTheDesktopEntryGroupOnly() {
  const QString document = QStringLiteral(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=Test Browser\n"
      "MimeType=text/html;x-scheme-handler/http;x-scheme-handler/https;\n"
      "Exec=test-browser %u\n");
  const QVector<QString> mimeTypes = desktopEntryMimeTypes(document);
  QCOMPARE(mimeTypes.size(), 3);
  QVERIFY(mimeTypes.contains(QStringLiteral("text/html")));
  QVERIFY(mimeTypes.contains(QStringLiteral("x-scheme-handler/http")));
  QVERIFY(mimeTypes.contains(QStringLiteral("x-scheme-handler/https")));
}

void DefaultApplicationsCatalogTest::
    ignoresAMimeTypeLikeKeyOutsideDesktopEntry() {
  const QString document = QStringLiteral(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=Test App\n"
      "Exec=test-app\n"
      "\n"
      "[Desktop Action Preview]\n"
      "MimeType=image/jpeg\n");
  QVERIFY(desktopEntryMimeTypes(document).isEmpty());
}

void DefaultApplicationsCatalogTest::filtersCandidatesByAnyCategoryMimeType() {
  DirectoryScan scan;
  ScannedApplication browser;
  browser.entry.id = QStringLiteral("browser");
  browser.entry.name = QStringLiteral("Test Browser");
  browser.documentText = QStringLiteral(
      "[Desktop Entry]\nMimeType=text/html;\n");
  ScannedApplication fileManager;
  fileManager.entry.id = QStringLiteral("filemanager");
  fileManager.entry.name = QStringLiteral("Test Files");
  fileManager.documentText = QStringLiteral(
      "[Desktop Entry]\nMimeType=inode/directory;\n");
  ScannedApplication both;
  both.entry.id = QStringLiteral("both");
  both.entry.name = QStringLiteral("Test Universal");
  both.documentText = QStringLiteral(
      "[Desktop Entry]\nMimeType=text/html;inode/directory;\n");
  scan.applications = {browser, fileManager, both};

  const QVector<QindaQt::Apps::SettingsDefaultApps::CandidateApplication>
      browserCandidates = candidateApplicationsForCategory(
          scan, DefaultApplicationCategory::Browser);
  QCOMPARE(browserCandidates.size(), 2);
  QStringList browserIds;
  for (const auto &candidate : browserCandidates) browserIds.append(candidate.id);
  QVERIFY(browserIds.contains(QStringLiteral("browser.desktop")));
  QVERIFY(browserIds.contains(QStringLiteral("both.desktop")));
  QVERIFY(!browserIds.contains(QStringLiteral("filemanager.desktop")));

  const QVector<QindaQt::Apps::SettingsDefaultApps::CandidateApplication>
      fileManagerCandidates = candidateApplicationsForCategory(
          scan, DefaultApplicationCategory::FileManager);
  QCOMPARE(fileManagerCandidates.size(), 2);
}

void DefaultApplicationsCatalogTest::pdfAndImageCategoriesAreIndependent() {
  DirectoryScan scan;
  ScannedApplication images;
  images.entry.id = QStringLiteral("images");
  images.documentText = QStringLiteral("[Desktop Entry]\nMimeType=image/jpeg;image/png;\n");
  ScannedApplication pdf;
  pdf.entry.id = QStringLiteral("pdf");
  pdf.documentText = QStringLiteral("[Desktop Entry]\nMimeType=application/pdf;\n");
  scan.applications = {images, pdf};
  const auto pdfs = candidateApplicationsForCategory(scan, DefaultApplicationCategory::PdfViewer);
  const auto viewers = candidateApplicationsForCategory(scan, DefaultApplicationCategory::ImageViewer);
  QCOMPARE(pdfs.size(), 1);
  QCOMPARE(pdfs.first().id, QStringLiteral("pdf.desktop"));
  QCOMPARE(viewers.size(), 1);
  QCOMPARE(viewers.first().id, QStringLiteral("images.desktop"));
}

void DefaultApplicationsCatalogTest::
    effectiveAssociationProjectionControlsCandidatesAndScope() {
  DirectoryScan scan;
  ScannedApplication added;
  added.entry.id = QStringLiteral("added");
  added.entry.name = QStringLiteral("Added");
  added.documentText = QStringLiteral("[Desktop Entry]\nMimeType=text/plain;\n");
  ScannedApplication removed;
  removed.entry.id = QStringLiteral("removed");
  removed.documentText = QStringLiteral("[Desktop Entry]\nMimeType=image/jpeg;\n");
  scan.applications = {added, removed};
  const QMap<QString, QStringList> associated{
      {QStringLiteral("added.desktop"), {QStringLiteral("text/plain"),
                                          QStringLiteral("image/png")}},
      {QStringLiteral("removed.desktop"), {}}};
  const auto candidates = candidateApplicationsForCategory(
      scan, DefaultApplicationCategory::ImageViewer, &associated);
  QCOMPARE(candidates.size(), 1);
  QCOMPARE(candidates.first().id, QStringLiteral("added.desktop"));
  QCOMPARE(candidates.first().supportedMimeTypes, QStringList{QStringLiteral("image/png")});
}

QTEST_GUILESS_MAIN(DefaultApplicationsCatalogTest)
#include "tst_default_applications_catalog.moc"
