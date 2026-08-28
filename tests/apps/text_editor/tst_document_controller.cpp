// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/document_controller.h"
#include "document/local_document_store.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::TextEditor;

namespace {

void writeText(const QString &path, const QByteArray &text) {
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(file.write(text), text.size());
}

} // namespace

class DocumentControllerTest final : public QObject {
  Q_OBJECT

private slots:
  void opensEditsAndSavesOneDocument();
  void externalReplacementBlocksSave();
  void saveAsRequiresExplicitReplacement();
  void samePathSaveAsHonorsReplacementConsent();
  void existingSymlinkAdoptsCanonicalTarget();
};

void DocumentControllerTest::opensEditsAndSavesOneDocument() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("notes.txt"));
  writeText(path, QByteArrayLiteral("hello"));
  DocumentController controller(std::make_unique<LocalDocumentStore>());
  QSignalSpy replacement(&controller,
                         &DocumentController::contentsReplacementRequested);

  QVERIFY(controller.openPath(path).ok());
  QCOMPARE(replacement.count(), 1);
  QCOMPARE(controller.state().text(), QStringLiteral("hello"));
  const DocumentOperation missing =
      controller.openPath(directory.filePath(QStringLiteral("missing.txt")));
  QCOMPARE(missing.error, DocumentError::NotFound);
  QCOMPARE(controller.state().path(), path);
  QCOMPARE(controller.state().text(), QStringLiteral("hello"));
  controller.setText(QStringLiteral("hello world"));
  QVERIFY(controller.state().isDirty());
  QVERIFY(controller.save().ok());
  QVERIFY(!controller.state().isDirty());

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), QByteArrayLiteral("hello world"));
}

void DocumentControllerTest::externalReplacementBlocksSave() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("notes.txt"));
  writeText(path, QByteArrayLiteral("baseline"));
  DocumentController controller(std::make_unique<LocalDocumentStore>());
  QVERIFY(controller.openPath(path).ok());
  controller.setText(QStringLiteral("local edit"));

  writeText(path, QByteArrayLiteral("outside edit"));
  controller.refreshExternalState();
  QCOMPARE(controller.state().externalState(), ExternalState::Changed);
  QCOMPARE(controller.save().error, DocumentError::ExternalConflict);
  QCOMPARE(controller.state().text(), QStringLiteral("local edit"));

  QVERIFY(QFile::remove(path));
  controller.refreshExternalState();
  QCOMPARE(controller.state().externalState(), ExternalState::Missing);
  QCOMPARE(controller.save().error, DocumentError::ExternalConflict);
}

void DocumentControllerTest::saveAsRequiresExplicitReplacement() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString existing = directory.filePath(QStringLiteral("existing.txt"));
  writeText(existing, QByteArrayLiteral("keep"));

  DocumentController controller(std::make_unique<LocalDocumentStore>());
  controller.setText(QStringLiteral("draft"));
  QCOMPARE(controller.saveAs(existing, false).error,
           DocumentError::DestinationExists);
  QVERIFY(controller.saveAs(existing, true).ok());
  QCOMPARE(controller.state().path(), existing);
  QVERIFY(!controller.state().isDirty());
}

void DocumentControllerTest::samePathSaveAsHonorsReplacementConsent() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("current.txt"));
  writeText(path, QByteArrayLiteral("baseline"));

  DocumentController controller(std::make_unique<LocalDocumentStore>());
  QVERIFY(controller.openPath(path).ok());
  controller.setText(QStringLiteral("local edit"));
  writeText(path, QByteArrayLiteral("outside edit"));
  controller.refreshExternalState();
  QCOMPARE(controller.state().externalState(), ExternalState::Changed);

  QCOMPARE(controller.saveAs(path, false).error,
           DocumentError::DestinationExists);
  QFile unchanged(path);
  QVERIFY(unchanged.open(QIODevice::ReadOnly));
  QCOMPARE(unchanged.readAll(), QByteArrayLiteral("outside edit"));
  unchanged.close();

  QVERIFY(controller.saveAs(path, true).ok());
  QCOMPARE(controller.state().externalState(), ExternalState::InSync);
  QFile replaced(path);
  QVERIFY(replaced.open(QIODevice::ReadOnly));
  QCOMPARE(replaced.readAll(), QByteArrayLiteral("local edit"));
}

void DocumentControllerTest::existingSymlinkAdoptsCanonicalTarget() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString target = directory.filePath(QStringLiteral("target.txt"));
  const QString link = directory.filePath(QStringLiteral("link.txt"));
  writeText(target, QByteArrayLiteral("baseline"));
  if (!QFile::link(target, link)) {
    QSKIP("The test filesystem does not support symbolic links");
  }

  DocumentController controller(std::make_unique<LocalDocumentStore>());
  QVERIFY(controller.openPath(link).ok());
  QCOMPARE(controller.state().path(), QFileInfo(target).canonicalFilePath());
  controller.setText(QStringLiteral("updated"));
  QVERIFY(controller.save().ok());
  QVERIFY(QFileInfo(link).isSymLink());

  QFile file(target);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), QByteArrayLiteral("updated"));
}

QTEST_GUILESS_MAIN(DocumentControllerTest)
#include "tst_document_controller.moc"
