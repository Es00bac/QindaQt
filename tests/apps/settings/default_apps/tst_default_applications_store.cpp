// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtTest>

using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationCategory;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationPreferences;
using QindaQt::Apps::SettingsDefaultApps::MimeAppsDefaultApplicationsStore;
using QindaQt::Apps::SettingsDefaultApps::defaultApplicationCategoryMimeTypes;

class DefaultApplicationsStoreTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void loadOnMissingFileReturnsEmptyPreferences();
  void savesEveryCategoryMimeTypeAndPreservesUnrelatedGroups();
  void emptyDesktopIdDeletesEveryMimeTypeKeyForThatCategory();
  void loadReadsOnlyTheRepresentativeMimeTypePerCategory();

private:
  std::unique_ptr<QTemporaryDir> m_dir;
  QString filePath() const {
    return QDir(m_dir->path()).filePath(QStringLiteral("mimeapps.list"));
  }
};

void DefaultApplicationsStoreTest::loadOnMissingFileReturnsEmptyPreferences() {
  m_dir = std::make_unique<QTemporaryDir>();
  QVERIFY(m_dir->isValid());
  MimeAppsDefaultApplicationsStore store(filePath());
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QVERIFY(preferences.browser.isEmpty());
  QVERIFY(preferences.fileManager.isEmpty());
}

void DefaultApplicationsStoreTest::
    savesEveryCategoryMimeTypeAndPreservesUnrelatedGroups() {
  m_dir = std::make_unique<QTemporaryDir>();
  QVERIFY(m_dir->isValid());
  // Simulate a file another tool already wrote to, with an unrelated group
  // and a mimetype no category in this route manages at all.
  QFile seed(filePath());
  QVERIFY(seed.open(QIODevice::WriteOnly | QIODevice::Text));
  seed.write("[Default Applications]\n"
             "application/pdf=okular.desktop\n"
             "\n"
             "[Added Associations]\n"
             "text/html=other-app.desktop;\n");
  seed.close();

  MimeAppsDefaultApplicationsStore store(filePath());
  DefaultApplicationPreferences preferences;
  // A save reflects the caller's full preference set (the model always
  // loads before mutating a copy); an empty mail default here is a real
  // "clear the mail default" intent, not an accidental wipe, so mail is
  // deliberately left out of this round-trip's assertions.
  preferences.browser = QStringLiteral("userapp-QindaFox.desktop");
  preferences.fileManager = QStringLiteral("org.qindaqt.FileManager.desktop");
  QString error;
  QVERIFY2(store.save(preferences, &error), qPrintable(error));

  QFile written(filePath());
  QVERIFY(written.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString contents = QString::fromUtf8(written.readAll());
  for (const QString &mimeType :
       defaultApplicationCategoryMimeTypes(DefaultApplicationCategory::Browser)) {
    QVERIFY2(contents.contains(mimeType + QStringLiteral("=userapp-QindaFox.desktop")),
             qPrintable(mimeType));
  }
  QVERIFY(contents.contains(
      QStringLiteral("inode/directory=org.qindaqt.FileManager.desktop")));
  // A mimetype no category manages stays untouched, and the
  // [Added Associations] group survives the write entirely.
  QVERIFY(contents.contains(QStringLiteral("application/pdf=okular.desktop")));
  QVERIFY(contents.contains(QStringLiteral("[Added Associations]")));
  QVERIFY(contents.contains(
      QStringLiteral("text/html=other-app.desktop")));
}

void DefaultApplicationsStoreTest::
    emptyDesktopIdDeletesEveryMimeTypeKeyForThatCategory() {
  m_dir = std::make_unique<QTemporaryDir>();
  QVERIFY(m_dir->isValid());
  MimeAppsDefaultApplicationsStore store(filePath());
  DefaultApplicationPreferences preferences;
  preferences.browser = QStringLiteral("userapp-QindaFox.desktop");
  QString error;
  QVERIFY2(store.save(preferences, &error), qPrintable(error));

  preferences.browser.clear();
  QVERIFY2(store.save(preferences, &error), qPrintable(error));

  QFile written(filePath());
  QVERIFY(written.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString contents = QString::fromUtf8(written.readAll());
  for (const QString &mimeType :
       defaultApplicationCategoryMimeTypes(DefaultApplicationCategory::Browser)) {
    QVERIFY2(!contents.contains(mimeType + QLatin1Char('=')), qPrintable(mimeType));
  }
}

void DefaultApplicationsStoreTest::
    loadReadsOnlyTheRepresentativeMimeTypePerCategory() {
  m_dir = std::make_unique<QTemporaryDir>();
  QVERIFY(m_dir->isValid());
  // text/html set, but the http/https siblings deliberately left pointing at
  // a different app -- a partially-foreign-edited group. Reading only the
  // representative (first) mimetype must not average or merge these.
  QFile seed(filePath());
  QVERIFY(seed.open(QIODevice::WriteOnly | QIODevice::Text));
  seed.write("[Default Applications]\n"
             "text/html=userapp-QindaFox.desktop\n"
             "x-scheme-handler/http=some-other-browser.desktop\n");
  seed.close();

  MimeAppsDefaultApplicationsStore store(filePath());
  DefaultApplicationPreferences preferences;
  QString error;
  QVERIFY2(store.load(&preferences, &error), qPrintable(error));
  QCOMPARE(preferences.browser, QStringLiteral("userapp-QindaFox.desktop"));
}

QTEST_GUILESS_MAIN(DefaultApplicationsStoreTest)
#include "tst_default_applications_store.moc"
