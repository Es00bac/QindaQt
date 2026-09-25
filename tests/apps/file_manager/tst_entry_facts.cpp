// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/column_listing.h"
#include "model/entry_facts.h"
#include "model/local_directory_lister.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool touch(const QString &path, const QByteArray &bytes = QByteArray("x")) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

[[nodiscard]] QStringList namesOf(const QVariantList &rows) {
  QStringList names;
  for (const QVariant &row : rows) {
    names.append(row.toMap().value(QStringLiteral("name")).toString());
  }
  return names;
}

} // namespace

// ADR-0270: the Details columns read for visible rows only (EntryFacts) and
// the Columns view's other columns (ColumnListing).
class TestEntryFacts final : public QObject {
  Q_OBJECT

private slots:
  void namesAnIdOrLeavesItUnknown();
  void countsAFoldersVisibleItemsInTheBackground();
  void readsDimensionsOfPreviewableImagesOnly();
  void neverReadsANetworkOrVirtualPath();
  void aChangedStampIsReadAgain();
  void columnListingOrdersAndHidesLikeTheWindow();
  void columnListingIsEmptyForAnythingButALocalFolder();
};

void TestEntryFacts::namesAnIdOrLeavesItUnknown() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  EntryFacts facts;
  QCOMPARE(facts.ownerName(-1), QString());
  QCOMPARE(facts.groupName(-1), QString());
  const QFileInfo folder(temporary.path());
  QCOMPARE(facts.ownerName(static_cast<qint64>(::getuid())), folder.owner());
  QCOMPARE(facts.groupName(static_cast<qint64>(folder.groupId())), folder.group());
}

void TestEntryFacts::countsAFoldersVisibleItemsInTheBackground() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString folder = temporary.filePath(QStringLiteral("folder"));
  QVERIFY(QDir().mkpath(folder));
  for (const char *name : {"a", "b", "c", ".hidden"}) {
    QVERIFY(touch(QDir(folder).filePath(QString::fromLatin1(name))));
  }
  EntryFacts facts;
  QSignalSpy revisions(&facts, &EntryFacts::revisionChanged);
  // Unknown until the one background read lands; then from the cache.
  QCOMPARE(facts.itemCount(folder, QStringLiteral("1")), QString());
  QTRY_COMPARE(facts.itemCount(folder, QStringLiteral("1")), QStringLiteral("3"));
  // The revision is published on a 50 ms debounce after the answer lands.
  QTRY_VERIFY(revisions.count() >= 1);
  QCOMPARE(facts.itemCount(temporary.filePath(QStringLiteral("missing")), QStringLiteral("1")),
           QString());
}

void TestEntryFacts::readsDimensionsOfPreviewableImagesOnly() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString image = temporary.filePath(QStringLiteral("wide.png"));
  QImage picture(20, 10, QImage::Format_RGB32);
  picture.fill(Qt::blue);
  QVERIFY(picture.save(image));
  const QString text = temporary.filePath(QStringLiteral("notes.png"));
  QVERIFY(touch(text, QByteArray("not an image")));

  EntryFacts facts;
  QTRY_COMPARE(facts.dimensions(image, QStringLiteral("1")), QStringLiteral("20 × 10"));
  QSignalSpy revisions(&facts, &EntryFacts::revisionChanged);
  QCOMPARE(facts.dimensions(text, QStringLiteral("1")), QString());
  QTRY_VERIFY(revisions.count() >= 1);
  // Read, and found to be no picture: still unknown, and no longer queued.
  QCOMPARE(facts.dimensions(text, QStringLiteral("1")), QString());
}

void TestEntryFacts::neverReadsANetworkOrVirtualPath() {
  EntryFacts facts;
  QSignalSpy revisions(&facts, &EntryFacts::revisionChanged);
  QCOMPARE(facts.itemCount(QStringLiteral("smb://server/share"), QStringLiteral("0")), QString());
  QCOMPARE(facts.dimensions(QStringLiteral("applications:editor"), QStringLiteral("0")), QString());
  QTest::qWait(200);
  QCOMPARE(revisions.count(), 0);
}

void TestEntryFacts::aChangedStampIsReadAgain() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString folder = temporary.filePath(QStringLiteral("folder"));
  QVERIFY(QDir().mkpath(folder));
  QVERIFY(touch(QDir(folder).filePath(QStringLiteral("one"))));
  EntryFacts facts;
  QTRY_COMPARE(facts.itemCount(folder, QStringLiteral("1")), QStringLiteral("1"));
  QVERIFY(touch(QDir(folder).filePath(QStringLiteral("two"))));
  // The same stamp is the cached answer; a new one reads the folder again.
  QCOMPARE(facts.itemCount(folder, QStringLiteral("1")), QStringLiteral("1"));
  QTRY_COMPARE(facts.itemCount(folder, QStringLiteral("2")), QStringLiteral("2"));
}

void TestEntryFacts::columnListingOrdersAndHidesLikeTheWindow() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString folder = temporary.filePath(QStringLiteral("folder"));
  QVERIFY(QDir().mkpath(QDir(folder).filePath(QStringLiteral("zeta"))));
  QVERIFY(touch(QDir(folder).filePath(QStringLiteral("beta.txt")), QByteArray("22")));
  QVERIFY(touch(QDir(folder).filePath(QStringLiteral("alpha.txt")), QByteArray("1")));
  QVERIFY(touch(QDir(folder).filePath(QStringLiteral(".secret"))));
  const ColumnListing listing(std::make_unique<LocalDirectoryLister>());

  const QVariantList rows = listing.children(folder, false, QStringLiteral("name"),
                                             QStringLiteral("ascending"), true);
  QCOMPARE(namesOf(rows), (QStringList{QStringLiteral("zeta"), QStringLiteral("alpha.txt"),
                                       QStringLiteral("beta.txt")}));
  const QVariantMap first = rows.constFirst().toMap();
  QCOMPARE(first.value(QStringLiteral("isDirectory")).toBool(), true);
  QCOMPARE(first.value(QStringLiteral("path")).toString(), QDir(folder).filePath(QStringLiteral("zeta")));
  QCOMPARE(first.value(QStringLiteral("iconName")).toString(), QStringLiteral("folder"));

  QCOMPARE(namesOf(listing.children(folder, true, QStringLiteral("size"),
                                    QStringLiteral("descending"), false)),
           (QStringList{QStringLiteral("beta.txt"), QStringLiteral(".secret"),
                        QStringLiteral("alpha.txt"), QStringLiteral("zeta")}));
}

void TestEntryFacts::columnListingIsEmptyForAnythingButALocalFolder() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const ColumnListing listing(std::make_unique<LocalDirectoryLister>());
  for (const QString &path : {QStringLiteral("smb://server/share"), QStringLiteral("applications:"),
                              temporary.filePath(QStringLiteral("missing")), QString()}) {
    QVERIFY2(listing.children(path, true, QStringLiteral("name"), QStringLiteral("ascending"), true)
                 .isEmpty(),
             qPrintable(path));
  }
}

QTEST_GUILESS_MAIN(TestEntryFacts)
#include "tst_entry_facts.moc"
