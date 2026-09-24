// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <utility>

using namespace QindaQt::Apps::SettingsDefaultApps;
using QindaQt::ApplicationCatalog::DirectoryScan;
using QindaQt::ApplicationCatalog::ScannedApplication;

// ADR-0269: File Manager's Open With reads and writes single MIME types
// through the same store Settings -> Default Applications uses.

namespace {

// Entry id -> the MimeType= list its desktop entry declares.
DirectoryScan applications() {
  DirectoryScan scan;
  const QList<std::pair<QString, QString>> entries{
      {QStringLiteral("editor"), QStringLiteral("text/plain;")},
      {QStringLiteral("writer"), QStringLiteral("text/plain;text/markdown;")},
      {QStringLiteral("notes"), QStringLiteral("image/png;")},
      {QStringLiteral("blocked"), QStringLiteral("text/plain;")},
      {QStringLiteral("viewer"), QStringLiteral("image/png;")}};
  for (const auto &[id, mimeTypes] : entries) {
    ScannedApplication application;
    application.entry.id = id;
    application.entry.name = id;
    application.documentText =
        QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%1\nMimeType=%2\n")
            .arg(id, mimeTypes);
    scan.applications.append(application);
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

MimeTypeHandlers handlersFor(DefaultApplicationsStore &store, const QString &mimeType) {
  MimeTypeHandlers handlers;
  QString error;
  if (!store.loadMimeTypeHandlers(mimeType, &handlers, &error)) {
    qWarning("loadMimeTypeHandlers failed: %s", qPrintable(error));
  }
  return handlers;
}

} // namespace

class DefaultApplicationsMimeTypesTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void handlersListTheDefaultThenAddedThenDeclared();
  void anUnassociatedDefaultIsSkipped();
  void savingADeclaredHandlerWritesOnlyThatType();
  void savingAnUndeclaredHandlerAlsoAssociatesIt();
  void desktopSpecificOverrideIsEditedInPlace();
  void refusesUnknownApplicationsAndMalformedTypes();
};

void DefaultApplicationsMimeTypesTest::handlersListTheDefaultThenAddedThenDeclared() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Default Applications]\ntext/plain=writer.desktop;\n"
                          "[Added Associations]\ntext/plain=notes.desktop;\n"
                          "[Removed Associations]\ntext/plain=blocked.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, applications());
  const MimeTypeHandlers handlers = handlersFor(store, QStringLiteral("text/plain"));
  QCOMPARE(handlers.defaultDesktopId, QStringLiteral("writer.desktop"));
  // The default, then the user's added association, then declared handlers
  // in scan order; a removed association and a non-handler never appear.
  QCOMPARE(handlers.desktopIds,
           QStringList({QStringLiteral("writer.desktop"), QStringLiteral("notes.desktop"),
                        QStringLiteral("editor.desktop")}));
}

void DefaultApplicationsMimeTypesTest::anUnassociatedDefaultIsSkipped() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  // viewer does not handle text/plain, so lookup passes over it (the MIME
  // Apps specification's rule, the same one load() applies to categories).
  QVERIFY(writeFile(path, "[Default Applications]\ntext/plain=viewer.desktop;editor.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, applications());
  const MimeTypeHandlers handlers = handlersFor(store, QStringLiteral("text/plain"));
  QCOMPARE(handlers.defaultDesktopId, QStringLiteral("editor.desktop"));
  QVERIFY(!handlers.desktopIds.contains(QStringLiteral("viewer.desktop")));
}

void DefaultApplicationsMimeTypesTest::savingADeclaredHandlerWritesOnlyThatType() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Default Applications]\nimage/png=viewer.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, applications());
  QString error;
  // A suffix-free id is written as a real desktop file ID.
  QVERIFY2(store.saveMimeTypeDefault(QStringLiteral("text/plain"), QStringLiteral("editor"),
                                     &error), qPrintable(error));
  const QString contents = readFile(path);
  QVERIFY(contents.contains(QStringLiteral("text/plain=editor.desktop;")));
  QVERIFY(contents.contains(QStringLiteral("image/png=viewer.desktop;")));
  QVERIFY(!contents.contains(QStringLiteral("Added Associations")));
  QVERIFY(!contents.contains(QStringLiteral("text/markdown")));
  QCOMPARE(handlersFor(store, QStringLiteral("text/plain")).defaultDesktopId,
           QStringLiteral("editor.desktop"));
}

void DefaultApplicationsMimeTypesTest::savingAnUndeclaredHandlerAlsoAssociatesIt() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  QVERIFY(writeFile(path, "[Added Associations]\ntext/plain=notes.desktop;\n"));
  MimeAppsDefaultApplicationsStore store(path, {path}, applications());
  QString error;
  // Other Application... may pick an app that does not declare the type.
  QVERIFY2(store.saveMimeTypeDefault(QStringLiteral("text/plain"),
                                     QStringLiteral("viewer.desktop"), &error),
           qPrintable(error));
  QVERIFY(readFile(path).contains(QStringLiteral("text/plain=viewer.desktop;notes.desktop;")));
  const MimeTypeHandlers handlers = handlersFor(store, QStringLiteral("text/plain"));
  QCOMPARE(handlers.defaultDesktopId, QStringLiteral("viewer.desktop"));
  QCOMPARE(handlers.desktopIds.mid(0, 2),
           QStringList({QStringLiteral("viewer.desktop"), QStringLiteral("notes.desktop")}));
}

void DefaultApplicationsMimeTypesTest::desktopSpecificOverrideIsEditedInPlace() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString generic = directory.filePath(QStringLiteral("mimeapps.list"));
  const QString desktop = directory.filePath(QStringLiteral("qindaqt-mimeapps.list"));
  QVERIFY(writeFile(desktop, "[Default Applications]\ntext/plain=writer.desktop;\n"));
  QVERIFY(writeFile(generic, "[Default Applications]\nimage/png=viewer.desktop;\n"));
  const QString genericBefore = readFile(generic);
  MimeAppsDefaultApplicationsStore store(generic, {desktop, generic}, applications(), {desktop});
  QString error;
  QVERIFY2(store.saveMimeTypeDefault(QStringLiteral("text/plain"),
                                     QStringLiteral("editor.desktop"), &error),
           qPrintable(error));
  // The desktop-specific file owned the key and outranks the generic one.
  QVERIFY(readFile(desktop).contains(QStringLiteral("text/plain=editor.desktop;")));
  QCOMPARE(readFile(generic), genericBefore);
  QCOMPARE(handlersFor(store, QStringLiteral("text/plain")).defaultDesktopId,
           QStringLiteral("editor.desktop"));
}

void DefaultApplicationsMimeTypesTest::refusesUnknownApplicationsAndMalformedTypes() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mimeapps.list"));
  MimeAppsDefaultApplicationsStore store(path, {path}, applications());
  QString error;
  QVERIFY(!store.saveMimeTypeDefault(QStringLiteral("text/plain"),
                                     QStringLiteral("missing.desktop"), &error));
  QCOMPARE(error, QStringLiteral("default-applications-unknown-application"));
  // A value that could become a group header, a second key or a list must
  // never reach the file.
  for (const QString &malformed :
       {QStringLiteral("text/plain\n[Default Applications]"), QStringLiteral("text"),
        QStringLiteral("a/b/c"), QStringLiteral("text/plain;x"), QStringLiteral("text/pl ain"),
        QStringLiteral("/plain"), QStringLiteral("text/")}) {
    QVERIFY2(!store.saveMimeTypeDefault(malformed, QStringLiteral("editor.desktop"), &error),
             qPrintable(malformed));
    QCOMPARE(error, QStringLiteral("default-applications-invalid-mime-type"));
    MimeTypeHandlers handlers;
    QVERIFY(!store.loadMimeTypeHandlers(malformed, &handlers, &error));
  }
  QVERIFY(!QFileInfo::exists(path));
}

QTEST_GUILESS_MAIN(DefaultApplicationsMimeTypesTest)
#include "tst_default_applications_mime_types.moc"
