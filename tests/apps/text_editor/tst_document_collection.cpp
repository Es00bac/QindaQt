// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/close_consent.h"
#include "document/document_collection.h"
#include "document/local_document_store.h"
#include "ui/document_title.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::TextEditor;

namespace {

DocumentStoreFactory localFactory() {
  return [] { return std::make_unique<LocalDocumentStore>(); };
}

bool writeFile(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace

class DocumentCollectionTest final : public QObject {
  Q_OBJECT

private slots:
  void pathsAreUniqueAndStateIsIndependent();
  void saveAsCannotAliasAnotherTab();
  void closePlanIsBoundedAndExplicit();
  void titlesAreSanitizedAndBounded();
};

void DocumentCollectionTest::pathsAreUniqueAndStateIsIndependent() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
  const QString secondPath = directory.filePath(QStringLiteral("second.txt"));
  QVERIFY(writeFile(firstPath, "first"));
  QVERIFY(writeFile(secondPath, "second"));

  DocumentCollection documents(localFactory());
  const AddDocumentResult first = documents.openPath(firstPath);
  const AddDocumentResult second = documents.openPath(secondPath);
  QVERIFY(first.ok());
  QVERIFY(second.ok());
  QCOMPARE(documents.count(), 2);

  const AddDocumentResult duplicate = documents.openPath(firstPath);
  QVERIFY(duplicate.ok());
  QVERIFY(duplicate.focusedExisting);
  QCOMPARE(duplicate.controller, first.controller);
  QCOMPARE(documents.count(), 2);

  first.controller->setText(QStringLiteral("first local"));
  QVERIFY(first.controller->state().isDirty());
  QVERIFY(!second.controller->state().isDirty());
  QCOMPARE(second.controller->state().text(), QStringLiteral("second"));

  QVERIFY(writeFile(firstPath, "first outside"));
  first.controller->refreshExternalState();
  second.controller->refreshExternalState();
  QCOMPARE(first.controller->state().externalState(), ExternalState::Changed);
  QCOMPARE(second.controller->state().externalState(), ExternalState::InSync);
}

void DocumentCollectionTest::saveAsCannotAliasAnotherTab() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("owned.txt"));
  QVERIFY(writeFile(path, "owned"));
  DocumentCollection documents(localFactory());
  QVERIFY(documents.openPath(path).ok());
  const AddDocumentResult untitled = documents.addUntitled();
  QVERIFY(untitled.ok());
  untitled.controller->setText(QStringLiteral("replacement"));
  const DocumentOperation collision = documents.saveAs(1, path, true);
  QCOMPARE(collision.error, DocumentError::AlreadyOpen);
  QCOMPARE(documents.count(), 2);
  QVERIFY(untitled.controller->state().isUntitled());
}

void DocumentCollectionTest::closePlanIsBoundedAndExplicit() {
  DocumentCollection documents(localFactory());
  for (int index = 0; index < 12; ++index) {
    AddDocumentResult added = documents.addUntitled();
    QVERIFY(added.ok());
    added.controller->setText(QStringLiteral("dirty %1").arg(index));
  }
  QList<DocumentController *> all;
  for (int index = 0; index < documents.count(); ++index) {
    all.append(documents.at(index));
  }
  const QString summary = boundedDirtyDocumentSummary(all);
  QVERIFY(summary.size() <= 512);
  QVERIFY(summary.contains(QStringLiteral("4 more")));
  QVERIFY(!makeClosePlan(all, CloseChoice::Cancel).proceed);
  QCOMPARE(makeClosePlan(all, CloseChoice::SaveAll).saveIndexes.size(), 12);
  QVERIFY(makeClosePlan(all, CloseChoice::DiscardAll).saveIndexes.isEmpty());
}

void DocumentCollectionTest::titlesAreSanitizedAndBounded() {
  QString hostile = QStringLiteral("  hello\n\tworld  ");
  hostile.append(QChar(0x0007));
  hostile.append(QString(200, u'x'));
  const QString title = sanitizeDocumentTitle(hostile);
  QVERIFY(title.size() <= 128);
  QVERIFY(!title.contains(u'\n'));
  QVERIFY(!title.contains(u'\t'));
  QVERIFY(!title.contains(QChar(0x0007)));
  QVERIFY(title.startsWith(QStringLiteral("hello world")));
}

QTEST_GUILESS_MAIN(DocumentCollectionTest)
#include "tst_document_collection.moc"
