// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/close_consent.h"
#include "document/document_collection.h"
#include "document/local_document_store.h"
#include "ui/document_title.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
  void saveAsResolvesSymlinkedParent();
  void closePlanIsBoundedAndExplicit();
  void titlesAreSanitizedAndBounded();
  void titleCollapseHonorsUtf16Boundary();
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

void DocumentCollectionTest::saveAsResolvesSymlinkedParent() {
  // AGENT-NOTE: P1-1 regression from the a13aa62 review: a not-yet-created
  // Save As target beneath a symlinked parent must retain the real identity.
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString realDirectory = directory.filePath(QStringLiteral("real"));
  const QString aliasDirectory = directory.filePath(QStringLiteral("alias"));
  QVERIFY(QDir().mkpath(realDirectory));
  QVERIFY(QFile::link(realDirectory, aliasDirectory));

  DocumentCollection documents(localFactory());
  const AddDocumentResult untitled = documents.addUntitled();
  QVERIFY(untitled.ok());
  untitled.controller->setText(QStringLiteral("shared"));
  const QString aliasPath =
      QDir(aliasDirectory).filePath(QStringLiteral("shared.txt"));
  QVERIFY(documents.saveAs(0, aliasPath, false).ok());

  const QString realPath =
      QDir(realDirectory).filePath(QStringLiteral("shared.txt"));
  QCOMPARE(untitled.controller->state().path(),
           QFileInfo(realPath).canonicalFilePath());
  const AddDocumentResult reopened = documents.openPath(realPath);
  QVERIFY(reopened.ok());
  QVERIFY(reopened.focusedExisting);
  QCOMPARE(reopened.controller, untitled.controller);
  QCOMPARE(documents.count(), 1);

  const QString unresolvable =
      QDir(directory.path())
          .filePath(QStringLiteral("missing-parent/refused.txt"));
  QCOMPARE(documents.saveAs(0, unresolvable, false).error,
           DocumentError::InvalidPath);
  QVERIFY(!QFileInfo::exists(unresolvable));
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

void DocumentCollectionTest::titleCollapseHonorsUtf16Boundary() {
  // AGENT-NOTE: P2-1 regression from the a13aa62 review: the collapsed space
  // and following scalar share the 128-unit budget; surrogate pairs stay whole.
  QCOMPARE(
      sanitizeDocumentTitle(QString(127, u'x') + QStringLiteral(" y")).size(),
      127);
  const QString supplementary = QString::fromUcs4(U"\U0001F680");
  const QString exact =
      sanitizeDocumentTitle(QString(125, u'x') + u' ' + supplementary);
  QCOMPARE(exact.size(), 128);
  QVERIFY(exact.isValidUtf16());
  const QString title =
      sanitizeDocumentTitle(QString(126, u'x') + u' ' + supplementary);
  QCOMPARE(title.size(), 126);
  QVERIFY(title.isValidUtf16());
}

QTEST_GUILESS_MAIN(DocumentCollectionTest)
#include "tst_document_collection.moc"
