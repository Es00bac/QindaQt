// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::TextEditor;

namespace {

void writeBytes(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
           qPrintable(file.errorString()));
  QCOMPARE(file.write(bytes), bytes.size());
}

QByteArray readBytes(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

} // namespace

class LocalDocumentStoreTest final : public QObject {
  Q_OBJECT

private slots:
  void loadsUtf8AndPreservesBomIntent();
  void roundTripsLineEndingPolicy_data();
  void roundTripsLineEndingPolicy();
  void rejectsInvalidAndOversizeInput();
  void protectsExistingAndExternallyChangedFiles();
  void writesAtomicallyAndPreservesBom();
};

void LocalDocumentStoreTest::loadsUtf8AndPreservesBomIntent() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("accent.txt"));
  writeBytes(path, QByteArray::fromHex("efbbbf") +
                       QStringLiteral("héllo\r\n").toUtf8());

  LocalDocumentStore store;
  const LoadResult result = store.load(path);
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(result.snapshot->text, QStringLiteral("héllo\n"));
  QVERIFY(result.snapshot->hasUtf8Bom);
  QCOMPARE(result.snapshot->lineEnding, LineEnding::CrLf);
  QCOMPARE(result.snapshot->revision.byteCount, QFileInfo(path).size());
  QCOMPARE(result.snapshot->revision.sha256.size(), 32);
}

void LocalDocumentStoreTest::roundTripsLineEndingPolicy_data() {
  QTest::addColumn<QByteArray>("input");
  QTest::addColumn<QString>("normalized");
  QTest::addColumn<int>("lineEnding");
  QTest::addColumn<QByteArray>("saved");

  QTest::newRow("lf-no-final-newline")
      << QByteArrayLiteral("alpha\nbeta") << QStringLiteral("alpha\nbeta")
      << static_cast<int>(LineEnding::Lf) << QByteArrayLiteral("alpha\nbeta");
  QTest::newRow("lf-final-newline")
      << QByteArrayLiteral("alpha\nbeta\n") << QStringLiteral("alpha\nbeta\n")
      << static_cast<int>(LineEnding::Lf) << QByteArrayLiteral("alpha\nbeta\n");
  QTest::newRow("crlf") << QByteArrayLiteral("alpha\r\nbeta\r\n")
                        << QStringLiteral("alpha\nbeta\n")
                        << static_cast<int>(LineEnding::CrLf)
                        << QByteArrayLiteral("alpha\r\nbeta\r\n");
  QTest::newRow("legacy-cr")
      << QByteArrayLiteral("alpha\rbeta\r") << QStringLiteral("alpha\nbeta\n")
      << static_cast<int>(LineEnding::Cr) << QByteArrayLiteral("alpha\rbeta\r");
  QTest::newRow("mixed-tie-prefers-crlf")
      << QByteArrayLiteral("alpha\r\nbeta\n") << QStringLiteral("alpha\nbeta\n")
      << static_cast<int>(LineEnding::CrLf)
      << QByteArrayLiteral("alpha\r\nbeta\r\n");
}

void LocalDocumentStoreTest::roundTripsLineEndingPolicy() {
  QFETCH(QByteArray, input);
  QFETCH(QString, normalized);
  QFETCH(int, lineEnding);
  QFETCH(QByteArray, saved);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString source = directory.filePath(QStringLiteral("source.txt"));
  const QString destination =
      directory.filePath(QStringLiteral("destination.txt"));
  writeBytes(source, input);

  LocalDocumentStore store;
  const LoadResult loaded = store.load(source);
  QVERIFY2(loaded.ok(), qPrintable(loaded.diagnostic));
  QCOMPARE(loaded.snapshot->text, normalized);
  QCOMPARE(static_cast<int>(loaded.snapshot->lineEnding), lineEnding);

  const SaveResult result = store.saveAtomic({
      .path = destination,
      .text = loaded.snapshot->text,
      .includeUtf8Bom = loaded.snapshot->hasUtf8Bom,
      .lineEnding = loaded.snapshot->lineEnding,
      .policy = SavePolicy::CreateOnly,
      .expectedRevision = std::nullopt,
  });
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(readBytes(destination), saved);
}

void LocalDocumentStoreTest::rejectsInvalidAndOversizeInput() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  LocalDocumentStore store;

  const QString invalid = directory.filePath(QStringLiteral("invalid.txt"));
  writeBytes(invalid, QByteArray::fromHex("c328"));
  QCOMPARE(store.load(invalid).error, DocumentError::InvalidUtf8);

  const QString large = directory.filePath(QStringLiteral("large.txt"));
  QFile file(large);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.resize(LocalDocumentStore::maximumDocumentBytes + 1));
  file.close();
  QCOMPARE(store.load(large).error, DocumentError::TooLarge);
  QCOMPARE(store.revision(large).error, DocumentError::TooLarge);
  QCOMPARE(store.load(directory.path()).error, DocumentError::NotRegularFile);

  QString invalidUtf16;
  invalidUtf16.append(QChar(0xD800));
  const QString invalidSave =
      directory.filePath(QStringLiteral("surrogate.txt"));
  QCOMPARE(store
               .saveAtomic({.path = invalidSave,
                            .text = invalidUtf16,
                            .policy = SavePolicy::CreateOnly,
                            .expectedRevision = std::nullopt})
               .error,
           DocumentError::InvalidUtf8);
  QVERIFY(!QFileInfo::exists(invalidSave));
}

void LocalDocumentStoreTest::protectsExistingAndExternallyChangedFiles() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("document.txt"));
  writeBytes(path, QByteArrayLiteral("first"));
  LocalDocumentStore store;
  const LoadResult loaded = store.load(path);
  QVERIFY(loaded.ok());

  const SaveResult createOnly = store.saveAtomic({
      .path = path,
      .text = QStringLiteral("replacement"),
      .policy = SavePolicy::CreateOnly,
      .expectedRevision = std::nullopt,
  });
  QCOMPARE(createOnly.error, DocumentError::DestinationExists);
  QCOMPARE(readBytes(path), QByteArrayLiteral("first"));

  writeBytes(path, QByteArrayLiteral("outside"));
  const SaveResult conflict = store.saveAtomic({
      .path = path,
      .text = QStringLiteral("local"),
      .policy = SavePolicy::MatchRevision,
      .expectedRevision = loaded.snapshot->revision,
  });
  QCOMPARE(conflict.error, DocumentError::ExternalConflict);
  QCOMPARE(readBytes(path), QByteArrayLiteral("outside"));
}

void LocalDocumentStoreTest::writesAtomicallyAndPreservesBom() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("new.txt"));
  LocalDocumentStore store;
  const SaveResult saved = store.saveAtomic({
      .path = path,
      .text = QStringLiteral("snowman: ☃\n"),
      .includeUtf8Bom = true,
      .lineEnding = LineEnding::CrLf,
      .policy = SavePolicy::CreateOnly,
      .expectedRevision = std::nullopt,
  });
  QVERIFY2(saved.ok(), qPrintable(saved.diagnostic));
  const QByteArray bytes = readBytes(path);
  QVERIFY(bytes.startsWith(QByteArray::fromHex("efbbbf")));
  QCOMPARE(bytes.mid(3), QStringLiteral("snowman: ☃\r\n").toUtf8());
  QCOMPARE(saved.revision->byteCount, qint64(bytes.size()));

  const QStringList leftovers =
      QDir(directory.path())
          .entryList({QStringLiteral("*.XXXXXX")}, QDir::Files | QDir::Hidden);
  QVERIFY(leftovers.isEmpty());
}

QTEST_APPLESS_MAIN(LocalDocumentStoreTest)
#include "tst_local_document_store.moc"
