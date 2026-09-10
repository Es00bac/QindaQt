// SPDX-License-Identifier: GPL-3.0-or-later
#include "restore/terminal_restore_store.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Apps::Terminal;

Q_DECLARE_METATYPE(QindaQt::Apps::Terminal::TerminalRestoreEntry)

namespace {

TerminalRestoreEntry makeEntry(const QString &profileId,
                               const QString &workingDirectory) {
  return TerminalRestoreEntry{profileId, workingDirectory};
}

const TerminalRestoreEntry kAlpha =
    makeEntry(QStringLiteral("builtin-default"), QStringLiteral("/home/user"));
const TerminalRestoreEntry kBeta =
    makeEntry(QStringLiteral("3f2504e0-4f89-11d3-9a0c-0305e82c3301"),
              QStringLiteral("/srv/build"));

} // namespace

class TestTerminalRestore final : public QObject {
  Q_OBJECT

private slots:
  void codecRoundTripsEntries();
  void encodeRefusesOverBoundList();
  void encodeRefusesHostileEntries_data();
  void encodeRefusesHostileEntries();
  void decodeMalformedDocumentFailsClosed();
  void decodeRefusesOverBoundArray();
  void decodeDropsHostileEntriesKeepsValid();
  void exitAppendDeduplicatesAndBounds();
  void planDispatchesOnlyAdmissibleEntries();
  void storeRoundTrip();
  void loadAbsentIsOkNotError();
  void loadMalformedFileIsTypedFailure();
  void loadOversizedFileIsTypedFailure();
  void clearRemovesStateFile();
};

void TestTerminalRestore::codecRoundTripsEntries() {
  const QList<TerminalRestoreEntry> entries{kAlpha, kBeta};
  bool ok = false;
  const QString json = encodeTerminalRestoreEntries(entries, &ok);
  QVERIFY(ok);
  QVERIFY(!json.isEmpty());
  const auto decoded = decodeTerminalRestoreEntries(json);
  QVERIFY(decoded.ok);
  QVERIFY(!decoded.absent);
  QCOMPARE(decoded.entries, entries);
}

void TestTerminalRestore::encodeRefusesOverBoundList() {
  QList<TerminalRestoreEntry> entries;
  for (int index = 0; index <= kMaxTerminalRestoreEntries; ++index) {
    entries.append(makeEntry(QStringLiteral("builtin-default"),
                             QStringLiteral("/home/user/%1").arg(index)));
  }
  bool ok = true;
  const QString json = encodeTerminalRestoreEntries(entries, &ok);
  QVERIFY(!ok);
  QVERIFY(json.isEmpty());
}

void TestTerminalRestore::encodeRefusesHostileEntries_data() {
  QTest::addColumn<TerminalRestoreEntry>("entry");
  QTest::addRow("relative-path")
      << makeEntry(QStringLiteral("builtin-default"),
                   QStringLiteral("relative/dir"));
  QTest::addRow("empty-path")
      << makeEntry(QStringLiteral("builtin-default"), QString());
  QTest::addRow("nul-in-path")
      << makeEntry(QStringLiteral("builtin-default"),
                   QStringLiteral("/home/user\0evil"));
  QTest::addRow("oversized-path")
      << makeEntry(QStringLiteral("builtin-default"),
                   QLatin1Char('/') + QString(kMaxTerminalRestorePathLength,
                                              QLatin1Char('a')));
  QTest::addRow("empty-profile-id")
      << makeEntry(QString(), QStringLiteral("/home/user"));
  QTest::addRow("hostile-profile-id")
      << makeEntry(QStringLiteral("../../etc/passwd"),
                   QStringLiteral("/home/user"));
  QTest::addRow("uppercase-profile-id")
      << makeEntry(QStringLiteral("Builtin-Default"),
                   QStringLiteral("/home/user"));
}

void TestTerminalRestore::encodeRefusesHostileEntries() {
  QFETCH(TerminalRestoreEntry, entry);
  bool ok = true;
  const QString json =
      encodeTerminalRestoreEntries(QList<TerminalRestoreEntry>{entry}, &ok);
  QVERIFY(!ok);
  QVERIFY(json.isEmpty());
}

void TestTerminalRestore::decodeMalformedDocumentFailsClosed() {
  const auto garbage = decodeTerminalRestoreEntries(QStringLiteral("{not json"));
  QVERIFY(!garbage.ok);
  QVERIFY(!garbage.diagnostic.isEmpty());
  QVERIFY(garbage.entries.isEmpty());
  // Well-formed JSON of the wrong shape is also fail-closed.
  const auto object = decodeTerminalRestoreEntries(QStringLiteral("{}"));
  QVERIFY(!object.ok);
  QVERIFY(object.entries.isEmpty());
}

void TestTerminalRestore::decodeRefusesOverBoundArray() {
  QJsonArray array;
  for (int index = 0; index <= kMaxTerminalRestoreEntries; ++index) {
    array.append(QJsonObject{
        {QStringLiteral("profileId"), QStringLiteral("builtin-default")},
        {QStringLiteral("workingDirectory"),
         QStringLiteral("/home/user/%1").arg(index)},
    });
  }
  const QJsonDocument document(array);
  const auto decoded = decodeTerminalRestoreEntries(
      QString::fromUtf8(document.toJson(QJsonDocument::Compact)));
  QVERIFY(!decoded.ok);
  QVERIFY(decoded.entries.isEmpty());
}

void TestTerminalRestore::decodeDropsHostileEntriesKeepsValid() {
  QJsonArray array;
  array.append(QJsonObject{
      {QStringLiteral("profileId"), kAlpha.profileId},
      {QStringLiteral("workingDirectory"), kAlpha.workingDirectory},
      {QStringLiteral("unknownFutureField"), 7},
  });
  array.append(QJsonObject{
      {QStringLiteral("profileId"), QStringLiteral("builtin-default")},
      {QStringLiteral("workingDirectory"), QStringLiteral("relative/dir")},
  });
  array.append(QJsonObject{
      {QStringLiteral("profileId"), QStringLiteral("bad id with spaces")},
      {QStringLiteral("workingDirectory"), QStringLiteral("/home/user")},
  });
  array.append(QJsonObject{
      {QStringLiteral("profileId"), 42},
      {QStringLiteral("workingDirectory"), QStringLiteral("/home/user")},
  });
  array.append(QStringLiteral("not an object"));
  array.append(QJsonObject{
      {QStringLiteral("profileId"), kBeta.profileId},
      {QStringLiteral("workingDirectory"), kBeta.workingDirectory},
  });
  const QJsonDocument document(array);
  const auto decoded = decodeTerminalRestoreEntries(
      QString::fromUtf8(document.toJson(QJsonDocument::Compact)));
  QVERIFY(decoded.ok);
  QCOMPARE(decoded.entries, (QList<TerminalRestoreEntry>{kAlpha, kBeta}));
}

void TestTerminalRestore::exitAppendDeduplicatesAndBounds() {
  // A re-closed window moves to the most-recent position instead of
  // duplicating.
  const auto merged = terminalRestoreWithExitAppended(
      QList<TerminalRestoreEntry>{kAlpha, kBeta}, kAlpha);
  QCOMPARE(merged, (QList<TerminalRestoreEntry>{kBeta, kAlpha}));

  // Beyond the bound the oldest entries are evicted.
  QList<TerminalRestoreEntry> full;
  for (int index = 0; index < kMaxTerminalRestoreEntries; ++index) {
    full.append(makeEntry(QStringLiteral("builtin-default"),
                          QStringLiteral("/home/user/%1").arg(index)));
  }
  const auto evicted = terminalRestoreWithExitAppended(full, kBeta);
  QCOMPARE(evicted.size(), kMaxTerminalRestoreEntries);
  QCOMPARE(evicted.constFirst(),
           makeEntry(QStringLiteral("builtin-default"),
                     QStringLiteral("/home/user/1")));
  QCOMPARE(evicted.constLast(), kBeta);
}

void TestTerminalRestore::planDispatchesOnlyAdmissibleEntries() {
  const auto noProfiles = [](const QString &) { return false; };
  const auto allProfiles = [](const QString &) { return true; };
  const auto noDirectories = [](const QString &) { return false; };
  const auto allDirectories = [](const QString &) { return true; };

  // Empty input restores nothing.
  const auto empty = planTerminalRestore({}, allProfiles, allDirectories);
  QVERIFY(!empty.hasPrimary);
  QVERIFY(empty.dispatched.isEmpty());

  // Unknown profiles and vanished directories are skipped; the first
  // admissible entry is the in-process primary, the rest dispatch.
  const TerminalRestoreEntry gamma =
      makeEntry(QStringLiteral("builtin-default"), QStringLiteral("/opt"));
  const QList<TerminalRestoreEntry> recorded{
      makeEntry(QStringLiteral("gone-profile"), QStringLiteral("/home/user")),
      makeEntry(QStringLiteral("builtin-default"),
                QStringLiteral("/vanished/dir")),
      kAlpha,
      kBeta,
      gamma,
  };
  const auto knownProfile = [&recorded](const QString &id) {
    return id != QStringLiteral("gone-profile");
  };
  const auto existingDirectory = [](const QString &path) {
    return path != QStringLiteral("/vanished/dir");
  };
  const auto plan =
      planTerminalRestore(recorded, knownProfile, existingDirectory);
  QVERIFY(plan.hasPrimary);
  QCOMPARE(plan.primary, kAlpha);
  QCOMPARE(plan.dispatched, (QList<TerminalRestoreEntry>{kBeta, gamma}));

  // Everything inadmissible means no primary at all.
  const auto none = planTerminalRestore(recorded, noProfiles, noDirectories);
  QVERIFY(!none.hasPrimary);
  QVERIFY(none.dispatched.isEmpty());
}

void TestTerminalRestore::storeRoundTrip() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const TerminalRestoreStore store(directory.path());
  const QList<TerminalRestoreEntry> entries{kAlpha, kBeta};
  const auto written = store.store(entries);
  QVERIFY2(written.ok, qPrintable(written.diagnostic));
  QVERIFY(QFile::exists(store.filePath()));
  const auto loaded = store.load();
  QVERIFY2(loaded.ok, qPrintable(loaded.diagnostic));
  QVERIFY(!loaded.absent);
  QCOMPARE(loaded.entries, entries);
  // State carries paths and profile ids only; it is not world-readable.
  const auto permissions = QFile::permissions(store.filePath());
  QVERIFY(!(permissions & QFileDevice::ReadGroup));
  QVERIFY(!(permissions & QFileDevice::ReadOther));
}

void TestTerminalRestore::loadAbsentIsOkNotError() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const TerminalRestoreStore store(
      QDir(directory.path()).filePath(QStringLiteral("never-created")));
  const auto loaded = store.load();
  QVERIFY(loaded.ok);
  QVERIFY(loaded.absent);
  QVERIFY(loaded.entries.isEmpty());
  QVERIFY(loaded.diagnostic.isEmpty());
}

void TestTerminalRestore::loadMalformedFileIsTypedFailure() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const TerminalRestoreStore store(directory.path());
  QFile file(store.filePath());
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.write("{not json at all") > 0);
  file.close();
  const auto loaded = store.load();
  QVERIFY(!loaded.ok);
  QVERIFY(!loaded.absent);
  QVERIFY(loaded.entries.isEmpty());
}

void TestTerminalRestore::loadOversizedFileIsTypedFailure() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const TerminalRestoreStore store(directory.path());
  QFile file(store.filePath());
  QVERIFY(file.open(QIODevice::WriteOnly));
  const QByteArray payload(int(TerminalRestoreStore::maximumBytes) + 1, ' ');
  QCOMPARE(file.write(payload), qint64(payload.size()));
  file.close();
  const auto loaded = store.load();
  QVERIFY(!loaded.ok);
  QVERIFY(!loaded.absent);
  QVERIFY(loaded.entries.isEmpty());
}

void TestTerminalRestore::clearRemovesStateFile() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const TerminalRestoreStore store(directory.path());
  // Clearing an absent file is not an error.
  QVERIFY(store.clear().ok);
  const auto written =
      store.store(QList<TerminalRestoreEntry>{kAlpha});
  QVERIFY(written.ok);
  QVERIFY(QFile::exists(store.filePath()));
  QVERIFY(store.clear().ok);
  QVERIFY(!QFile::exists(store.filePath()));
  const auto loaded = store.load();
  QVERIFY(loaded.ok);
  QVERIFY(loaded.absent);
}

QTEST_MAIN(TestTerminalRestore)
#include "tst_terminal_restore.moc"
