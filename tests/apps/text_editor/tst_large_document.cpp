// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/document_controller.h"
#include "document/local_document_store.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::TextEditor;

class LargeDocumentTest final : public QObject {
  Q_OBJECT

private slots:
  void opensEditsAndAtomicallySavesEightMiB();
};

void LargeDocumentTest::opensEditsAndAtomicallySavesEightMiB() {
  constexpr qsizetype payloadBytes = 8 * 1024 * 1024;
  constexpr qint64 operationLimitMs = 5'000;
  constexpr qint64 incrementalEditLimitMs = 500;

  QByteArray payload;
  payload.reserve(payloadBytes);
  const QByteArray line = QByteArrayLiteral(
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ text\n");
  while (payload.size() < payloadBytes) {
    payload.append(line);
  }
  payload.truncate(payloadBytes);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("large.txt"));
  QFile seed(path);
  QVERIFY(seed.open(QIODevice::WriteOnly));
  QCOMPARE(seed.write(payload), payload.size());
  seed.close();

  DocumentController controller(std::make_unique<LocalDocumentStore>());
  QElapsedTimer timer;
  timer.start();
  const DocumentOperation opened = controller.openPath(path);
  const qint64 openMs = timer.elapsed();
  QVERIFY2(opened.ok(), qPrintable(opened.diagnostic));
  QCOMPARE(controller.state().text().size(), payloadBytes);

  timer.restart();
  controller.applyTextEdit(controller.state().text().size(), 0,
                           QStringLiteral("tail\n"));
  const qint64 editMs = timer.elapsed();
  QVERIFY(controller.state().isDirty());
  QVERIFY(controller.state().text().endsWith(QStringLiteral("tail\n")));

  timer.restart();
  const DocumentOperation saved = controller.save();
  const qint64 saveMs = timer.elapsed();
  QVERIFY2(saved.ok(), qPrintable(saved.diagnostic));

  QFile result(path);
  QVERIFY(result.open(QIODevice::ReadOnly));
  QCOMPARE(result.size(), qint64(payloadBytes + 5));
  QVERIFY(result.seek(result.size() - 5));
  QCOMPARE(result.readAll(), QByteArrayLiteral("tail\n"));

  qInfo("8 MiB editor row: open=%lld ms incremental-edit=%lld ms save=%lld ms",
        static_cast<long long>(openMs), static_cast<long long>(editMs),
        static_cast<long long>(saveMs));
  QVERIFY2(openMs <= operationLimitMs, "8 MiB open exceeded 5 seconds");
  QVERIFY2(editMs <= incrementalEditLimitMs,
           "8 MiB incremental edit exceeded 500 milliseconds");
  QVERIFY2(saveMs <= operationLimitMs, "8 MiB save exceeded 5 seconds");
}

QTEST_GUILESS_MAIN(LargeDocumentTest)
#include "tst_large_document.moc"
