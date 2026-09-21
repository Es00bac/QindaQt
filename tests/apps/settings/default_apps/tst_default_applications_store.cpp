// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Apps::SettingsDefaultApps;
using QindaQt::ApplicationCatalog::DirectoryScan;
using QindaQt::ApplicationCatalog::ScannedApplication;

namespace {
DirectoryScan installedApplications() {
  DirectoryScan scan;
  for (const QString &id : {QStringLiteral("browser.desktop"),
                           QStringLiteral("other.desktop"),
                           QStringLiteral("viewer.desktop")}) {
    ScannedApplication app;
    app.entry.id = id.chopped(8);
    app.documentText = QStringLiteral(
        "[Desktop Entry]\nMimeType=text/html;x-scheme-handler/http;"
        "x-scheme-handler/https;x-scheme-handler/mailto;inode/directory;text/plain;application/pdf;"
        "image/jpeg;image/png;image/gif;image/webp;image/bmp;image/tiff;"
        "image/svg+xml;video/mp4;audio/mpeg;\n");
    scan.applications.append(app);
  }
  return scan;
}

bool writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QString readFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}
} // namespace

class DefaultApplicationsStoreTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void missingFilesReturnEmptyPreferences();
  void categoryWritePreservesOtherCategoriesAndAssociations();
  void clearRevealsInheritedDefault();
  void categoryWriteUpdatesEveryMimeType_data();
  void categoryWriteUpdatesEveryMimeType();
  void loadReadsRepresentativeMimeType();
  void orderedFallbackSkipsUninstalledAndRemovedApplications();
  void addedAssociationCanSupplyAnInheritedDefault();
  void lowerPriorityAssociationCannotRemoveUserDesktopEntry();
  void writesExistingDesktopSpecificUserOverride();
  void buildsFreedesktopLookupOrder();
  void rejectsUnknownApplicationWithoutWriting();
  void removedApplicationCannotBecomeAnIneffectiveDefault();
  void hiddenDesktopEntryDoesNotMaskInstalledFallback();
  void noDisplayHandlerRemainsTheConfiguredDefault();
  void legacySuffixFreeValueIsReadAndReportedCanonically();
  void aSuffixFreeChoiceIsWrittenWithTheSuffix();
  void repairingOneCategoryLeavesOtherLegacyValuesReadable();
};

void DefaultApplicationsStoreTest::missingFilesReturnEmptyPreferences() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  DefaultApplicationPreferences preferences;
  preferences.pdfViewer = QStringLiteral("stale.desktop");
  QString error;
  QVERIFY2(store.load(&preferences, &error), qPrintable(error));
  for (const auto category : kDefaultApplicationCategories)
    QVERIFY(preferences.category(category).isEmpty());
  QVERIFY(!QFileInfo::exists(path));
}

void DefaultApplicationsStoreTest::categoryWritePreservesOtherCategoriesAndAssociations() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  // An external change made after the page loaded must survive, including
  // another managed category and a split image-category preference.
  QVERIFY(writeFile(path,
      "[Default Applications]\napplication/pdf=other.desktop;\n"
      "image/jpeg=viewer.desktop;\nimage/png=other.desktop;\n"
      "application/x-example=unmanaged.desktop;\n"
      "[Added Associations]\ntext/html=other.desktop;\n"
      "[Removed Associations]\nimage/jpeg=blocked.desktop;\n"));
  QVERIFY2(store.saveCategory(DefaultApplicationCategory::Browser,
                             QStringLiteral("browser.desktop"), &error), qPrintable(error));
  const QString contents = readFile(path);
  for (const QString &mimeType : defaultApplicationCategoryMimeTypes(
           DefaultApplicationCategory::Browser))
    QVERIFY(contents.contains(mimeType + QStringLiteral("=browser.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("application/pdf=other.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("image/jpeg=viewer.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("image/png=other.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("application/x-example=unmanaged.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("[Added Associations]")));
  QVERIFY(contents.contains(QStringLiteral("text/html=other.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("[Removed Associations]")));
  QVERIFY(contents.contains(QStringLiteral("image/jpeg=blocked.desktop;")));
  QVERIFY(!contents.contains(QStringLiteral("inode/directory=")));
}

void DefaultApplicationsStoreTest::clearRevealsInheritedDefault() {
  QTemporaryDir directory;
  const QString user = directory.filePath(QStringLiteral("mimeapps.list"));
  const QString packaged = directory.filePath(QStringLiteral("qindaqt-mimeapps.list"));
  QVERIFY(writeFile(packaged, "[Default Applications]\napplication/pdf=viewer.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(user, {user, packaged}, installedApplications());
  QString error;
  QVERIFY(store.saveCategory(DefaultApplicationCategory::PdfViewer,
                             QStringLiteral("other.desktop"), &error));
  DefaultApplicationPreferences preferences;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("other.desktop"));
  QVERIFY(store.saveCategory(DefaultApplicationCategory::PdfViewer, {}, &error));
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("viewer.desktop"));
  QVERIFY(!readFile(user).contains(QStringLiteral("application/pdf=")));
  QVERIFY(!readFile(user).contains(QStringLiteral("image/jpeg=")));
}

void DefaultApplicationsStoreTest::categoryWriteUpdatesEveryMimeType_data() {
  QTest::addColumn<int>("categoryValue");
  for (const auto category : kDefaultApplicationCategories)
    QTest::newRow(qPrintable(defaultApplicationCategoryId(category))) << static_cast<int>(category);
}

void DefaultApplicationsStoreTest::categoryWriteUpdatesEveryMimeType() {
  QFETCH(int, categoryValue);
  const auto category = static_cast<DefaultApplicationCategory>(categoryValue);
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  QString error;
  QVERIFY2(store.saveCategory(category, QStringLiteral("viewer.desktop"), &error), qPrintable(error));
  const QString saved = readFile(path);
  for (const QString &mimeType : defaultApplicationCategoryMimeTypes(category))
    QVERIFY2(saved.contains(mimeType + QStringLiteral("=viewer.desktop;")), qPrintable(mimeType));
  QVERIFY(store.saveCategory(category, {}, &error));
  const QString cleared = readFile(path);
  for (const QString &mimeType : defaultApplicationCategoryMimeTypes(category))
    QVERIFY(!cleared.contains(mimeType + QLatin1Char('=')));
}

void DefaultApplicationsStoreTest::loadReadsRepresentativeMimeType() {
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Default Applications]\ntext/html=browser.desktop;\n"
                         "x-scheme-handler/http=other.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.browser, QStringLiteral("browser.desktop"));
}

void DefaultApplicationsStoreTest::orderedFallbackSkipsUninstalledAndRemovedApplications() {
  QTemporaryDir directory;
  const QString user = directory.filePath(QStringLiteral("mimeapps.list"));
  const QString admin = directory.filePath(QStringLiteral("admin-mimeapps.list"));
  const QString packaged = directory.filePath(QStringLiteral("qindaqt-mimeapps.list"));
  QVERIFY(writeFile(user, "[Default Applications]\napplication/pdf=missing.desktop;other.desktop;\n"
                         "[Removed Associations]\napplication/pdf=other.desktop;\n"));
  QVERIFY(writeFile(admin, "[Default Applications]\napplication/pdf=absent.desktop;viewer.desktop;\n"));
  QVERIFY(writeFile(packaged, "[Default Applications]\napplication/pdf=browser.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(user, {user, admin, packaged}, installedApplications());
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("viewer.desktop"));
}

void DefaultApplicationsStoreTest::addedAssociationCanSupplyAnInheritedDefault() {
  QTemporaryDir directory;
  const QString user = directory.filePath(QStringLiteral("mimeapps.list"));
  auto scan = installedApplications();
  scan.applications.first().documentText = QStringLiteral("[Desktop Entry]\nMimeType=text/html;\n");
  QVERIFY(writeFile(user, "[Default Applications]\napplication/pdf=browser.desktop;\n"
                         "[Added Associations]\napplication/pdf=browser.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(user, {user}, scan);
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("browser.desktop"));
}

void DefaultApplicationsStoreTest::lowerPriorityAssociationCannotRemoveUserDesktopEntry() {
  QTemporaryDir directory;
  const QString user = directory.filePath(QStringLiteral("config/mimeapps.list"));
  const QString home = directory.filePath(QStringLiteral("home/applications/mimeapps.list"));
  const QString system = directory.filePath(QStringLiteral("system/applications/mimeapps.list"));
  for (const QString &path : {user, home, system}) QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
  QVERIFY(writeFile(user, "[Default Applications]\napplication/pdf=viewer.desktop;\n"));
  QVERIFY(writeFile(system, "[Removed Associations]\napplication/pdf=viewer.desktop;\n"));
  auto scan = installedApplications();
  scan.applications.last().desktopFilePath = directory.filePath(
      QStringLiteral("home/applications/viewer.desktop"));
  MimeAppsDefaultApplicationsStore store(user, {user, home, system}, scan);
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("viewer.desktop"));
}

void DefaultApplicationsStoreTest::writesExistingDesktopSpecificUserOverride() {
  QTemporaryDir directory;
  const QString generic = directory.filePath(QStringLiteral("mimeapps.list"));
  const QString desktop = directory.filePath(QStringLiteral("qindaqt-mimeapps.list"));
  QVERIFY(writeFile(desktop, "[Default Applications]\napplication/pdf=viewer.desktop;\n"
                            "image/jpeg=viewer.desktop;\n"));
  QVERIFY(writeFile(generic, "[Default Applications]\napplication/pdf=browser.desktop;\n"));
  const QString genericBefore = readFile(generic);
  MimeAppsDefaultApplicationsStore store(generic, {desktop, generic}, installedApplications(), {desktop});
  QString error;
  QVERIFY(store.saveCategory(DefaultApplicationCategory::PdfViewer,
                             QStringLiteral("other.desktop"), &error));
  DefaultApplicationPreferences preferences;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("other.desktop"));
  QCOMPARE(readFile(generic), genericBefore);
  QVERIFY(readFile(desktop).contains(QStringLiteral("image/jpeg=viewer.desktop;")));
  QVERIFY(store.saveCategory(DefaultApplicationCategory::PdfViewer, {}, &error));
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("browser.desktop"));
}

void DefaultApplicationsStoreTest::buildsFreedesktopLookupOrder() {
  const QStringList paths = defaultApplicationsLookupPaths(
      {QStringLiteral("/user/config"), QStringLiteral("/admin/config")},
      {QStringLiteral("/user/data"), QStringLiteral("/system/data")},
      {QStringLiteral("QindaQt"), QStringLiteral("KDE"), QStringLiteral("QindaQt"),
       QStringLiteral("../bad")});
  QCOMPARE(paths.size(), 12);
  QCOMPARE(paths.at(0), QStringLiteral("/user/config/qindaqt-mimeapps.list"));
  QCOMPARE(paths.at(1), QStringLiteral("/user/config/kde-mimeapps.list"));
  QCOMPARE(paths.at(2), QStringLiteral("/user/config/mimeapps.list"));
  QCOMPARE(paths.at(3), QStringLiteral("/admin/config/qindaqt-mimeapps.list"));
  QCOMPARE(paths.at(6), QStringLiteral("/user/data/applications/qindaqt-mimeapps.list"));
  QCOMPARE(paths.at(11), QStringLiteral("/system/data/applications/mimeapps.list"));
}

void DefaultApplicationsStoreTest::rejectsUnknownApplicationWithoutWriting() {
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  QString error;
  QVERIFY(!store.saveCategory(DefaultApplicationCategory::PdfViewer,
                             QStringLiteral("missing.desktop"), &error));
  QVERIFY(!error.isEmpty());
  QVERIFY(!QFileInfo::exists(path));
}

void DefaultApplicationsStoreTest::removedApplicationCannotBecomeAnIneffectiveDefault() {
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Removed Associations]\napplication/pdf=viewer.desktop;\n"));
  const QString before = readFile(path);
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  QString error;
  QVERIFY(!store.saveCategory(DefaultApplicationCategory::PdfViewer,
                             QStringLiteral("viewer.desktop"), &error));
  QCOMPARE(error, QStringLiteral("default-applications-unsupported-application"));
  QCOMPARE(readFile(path), before);
}

void DefaultApplicationsStoreTest::hiddenDesktopEntryDoesNotMaskInstalledFallback() {
  QTemporaryDir directory;
  const QString user = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("applications"))));
  QVERIFY(writeFile(directory.filePath(QStringLiteral("applications/hidden.desktop")),
      "[Desktop Entry]\nType=Application\nName=Hidden viewer\nExec=/bin/true %U\n"
      "MimeType=application/pdf;\nHidden=true\n"));
  QVERIFY(writeFile(directory.filePath(QStringLiteral("applications/viewer.desktop")),
      "[Desktop Entry]\nType=Application\nName=Visible viewer\nExec=/bin/true %U\n"
      "MimeType=application/pdf;\n"));
  QVERIFY(writeFile(user, "[Default Applications]\napplication/pdf=hidden.desktop;viewer.desktop;\n"));
  auto scan = QindaQt::ApplicationCatalog::scanApplicationDirectories({directory.path()});
  QCOMPARE(scan.applications.size(), 1);
  QCOMPARE(scan.applications.first().entry.id, QStringLiteral("viewer"));
  MimeAppsDefaultApplicationsStore store(user, {user}, scan);
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.pdfViewer, QStringLiteral("viewer.desktop"));
  // The same real scanner identity must round-trip on an explicit choice.
  QVERIFY(store.saveCategory(DefaultApplicationCategory::PdfViewer,
                             QStringLiteral("viewer.desktop"), &error));
}

void DefaultApplicationsStoreTest::noDisplayHandlerRemainsTheConfiguredDefault() {
  QTemporaryDir directory;
  const QString user = directory.filePath(QStringLiteral("mimeapps.list"));
  const QString packaged = directory.filePath(QStringLiteral("qindaqt-mimeapps.list"));
  QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("applications"))));
  QVERIFY(writeFile(directory.filePath(QStringLiteral("applications/nodisplay.desktop")),
      "[Desktop Entry]\nType=Application\nName=NoDisplay browser\nExec=/bin/true %U\n"
      "MimeType=text/html;x-scheme-handler/http;x-scheme-handler/https;\nNoDisplay=true\n"));
  QVERIFY(writeFile(directory.filePath(QStringLiteral("applications/visible.desktop")),
      "[Desktop Entry]\nType=Application\nName=Visible browser\nExec=/bin/true %U\n"
      "MimeType=text/html;x-scheme-handler/http;x-scheme-handler/https;\n"));
  QVERIFY(writeFile(user, "[Default Applications]\ntext/html=nodisplay.desktop;\n"));
  QVERIFY(writeFile(packaged, "[Default Applications]\ntext/html=visible.desktop;\n"));
  const auto scan = QindaQt::ApplicationCatalog::scanApplicationDirectories(
      {directory.path()}, QindaQt::ApplicationCatalog::ApplicationVisibility::IncludeNoDisplay);
  MimeAppsDefaultApplicationsStore store(user, {user, packaged}, scan);
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY2(store.load(&preferences, &error), qPrintable(error));
  QCOMPARE(preferences.browser, QStringLiteral("nodisplay.desktop"));
  QVERIFY2(store.saveCategory(DefaultApplicationCategory::Browser,
                             QStringLiteral("nodisplay.desktop"), &error), qPrintable(error));
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.browser, QStringLiteral("nodisplay.desktop"));
}

void DefaultApplicationsStoreTest::legacySuffixFreeValueIsReadAndReportedCanonically() {
  // Real files on both machines carried values with no ".desktop" suffix, and
  // lookup used to refuse them - so the page showed "no default" for four
  // categories while the file plainly had values. Read them, and report the
  // canonical spelling so the catalog's name lookup still matches.
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Default Applications]\n"
                         "text/plain=other\n"
                         "text/html=browser.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY2(store.load(&preferences, &error), qPrintable(error));
  QCOMPARE(preferences.textEditor, QStringLiteral("other.desktop"));
  // The correctly spelled neighbour is untouched.
  QCOMPARE(preferences.browser, QStringLiteral("browser.desktop"));
}

void DefaultApplicationsStoreTest::aSuffixFreeChoiceIsWrittenWithTheSuffix() {
  // Tolerating a suffix-free value on read must not let one round-trip back
  // into the file, or the defect repairs itself into existence again.
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Default Applications]\ntext/plain=other\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  QString error;
  QVERIFY2(store.saveCategory(DefaultApplicationCategory::TextEditor,
                              QStringLiteral("viewer"), &error),
           qPrintable(error));
  const QString written = readFile(path);
  QVERIFY2(written.contains(QStringLiteral("text/plain=viewer.desktop;")),
           qPrintable(written));

  // And a canonical choice stays canonical rather than doubling the suffix.
  QVERIFY2(store.saveCategory(DefaultApplicationCategory::TextEditor,
                              QStringLiteral("other.desktop"), &error),
           qPrintable(error));
  const QString again = readFile(path);
  QVERIFY2(again.contains(QStringLiteral("text/plain=other.desktop;")),
           qPrintable(again));
  QVERIFY2(!again.contains(QStringLiteral(".desktop.desktop")), qPrintable(again));
}

void DefaultApplicationsStoreTest::repairingOneCategoryLeavesOtherLegacyValuesReadable() {
  // The page repairs a category when the user chooses in it. Until they do,
  // every other legacy value must still read - the repair is per-category and
  // must not depend on being performed.
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Default Applications]\n"
                         "text/plain=other\n"
                         "image/jpeg=viewer\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, installedApplications());
  QString error;
  QVERIFY2(store.saveCategory(DefaultApplicationCategory::TextEditor,
                              QStringLiteral("browser.desktop"), &error),
           qPrintable(error));
  DefaultApplicationPreferences preferences;
  QVERIFY2(store.load(&preferences, &error), qPrintable(error));
  QCOMPARE(preferences.textEditor, QStringLiteral("browser.desktop"));
  QCOMPARE(preferences.imageViewer, QStringLiteral("viewer.desktop"));
}

QTEST_GUILESS_MAIN(DefaultApplicationsStoreTest)
#include "tst_default_applications_store.moc"
