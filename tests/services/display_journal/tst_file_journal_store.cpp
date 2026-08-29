// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_journal/file_journal_store.h>

#include "file_journal_hooks_p.h"

#include <qindaqt/services/display_topology/topology.h>
#include <qindaqt/services/display_transaction/transaction_journal.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <sys/stat.h>
#include <unistd.h>

#include <optional>

using namespace QindaQt::DisplayJournal;
namespace Display = QindaQt::Display;
namespace DisplayTopology = QindaQt::DisplayTopology;
namespace DisplayTransaction = QindaQt::DisplayTransaction;

namespace {

constexpr char kJournalName[] = "display1-transaction.journal";
constexpr char kTemporaryName[] = ".display1-transaction.journal.tmp";

Display::Output output() {
  const Display::Mode mode{.id = QStringLiteral("current:1920x1080@60000"),
                           .pixelSize = QSize(1920, 1080),
                           .refreshMilliHertz = 60'000,
                           .preferred = true};
  return {.stableId = QStringLiteral("conn:Virtual-1"),
          .connectorName = QStringLiteral("Virtual-1"),
          .runtimeCompositorUuid = QStringLiteral("runtime"),
          .label = QStringLiteral("Virtual output"),
          .manufacturer = QStringLiteral("QIN"),
          .model = QStringLiteral("Reference"),
          .physicalSizeMillimeters = QSize(600, 340),
          .hasSerial = false,
          .internal = false,
          .ambiguousIdentity = false,
          .enabled = true,
          .primary = true,
          .modeId = mode.id,
          .position = {},
          .logicalSize = QSize(1920, 1080),
          .scale = 1.0,
          .transform = Display::Transform::Normal,
          .priority = 1,
          .replicationSourceStableId = {},
          .modes = {mode},
          .wireValid = true};
}

DisplayTransaction::Journal journal(const QString &transactionId,
                                    const QPoint targetPosition = {}) {
  Display::Snapshot snapshot{.protocolVersion = Display::kProtocolVersion,
                             .serviceEpoch = QStringLiteral("epoch"),
                             .revision = 7,
                             .liveFingerprint = {},
                             .outputs = {output()},
                             .transactions = {},
                             .wireValid = true};
  snapshot.liveFingerprint = DisplayTopology::canonicalFingerprint(
      DisplayTopology::candidateFromSnapshot(snapshot));
  Display::Candidate target = DisplayTopology::candidateFromSnapshot(snapshot);
  target.outputs[0].position = targetPosition;
  return {.schemaVersion = DisplayTransaction::kJournalSchemaVersion,
          .transactionId = transactionId,
          .phase = DisplayTransaction::JournalPhase::AwaitingConfirmation,
          .reason = Display::TransactionReason::TransportUncertain,
          .preimage = DisplayTopology::candidateFromSnapshot(snapshot),
          .target = std::move(target),
          .revertAttempt = 0};
}

bool writeFixture(const QString &path, const QByteArray &payload,
                  const QFileDevice::Permissions permissions =
                      QFileDevice::ReadOwner | QFileDevice::WriteOwner) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file.write(payload) != payload.size() || !file.flush()) {
    return false;
  }
  file.close();
  return QFile::setPermissions(path, permissions);
}

class UnavailableOutputPort final
    : public QindaQt::DisplayWriter::OutputManagementPort {
public:
  void
  setObserver(QindaQt::DisplayWriter::OutputManagementObserver *) override {}
  [[nodiscard]] QindaQt::DisplayWriter::PortStartStatus start() override {
    return QindaQt::DisplayWriter::PortStartStatus::ConnectionUnavailable;
  }
  void stop() override {}
  [[nodiscard]] QindaQt::DisplayWriter::SubmitStatus
  submit(const QindaQt::DisplayWriter::Configuration &) override {
    return QindaQt::DisplayWriter::SubmitStatus::Unavailable;
  }
};

class InjectedJournalHooks final : public Private::FileJournalHooks {
public:
  void beforeOpenJournal() override {
    if (growPath.isEmpty()) {
      return;
    }
    growthAttempted = true;
    QFile file(growPath);
    growthSucceeded = file.open(QIODevice::ReadWrite) && file.resize(1LL << 40);
    growPath.clear();
  }

  [[nodiscard]] std::optional<bool> directorySyncResult() override {
    const std::optional<bool> result = nextDirectorySync;
    nextDirectorySync.reset();
    return result;
  }

  QString growPath;
  std::optional<bool> nextDirectorySync;
  bool growthAttempted = false;
  bool growthSucceeded = false;
};

} // namespace

class FileJournalStoreTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void storesLoadsReplacesAndClears();
  void interruptedTemporaryPreservesCommittedValue();
  void rejectsUnsafeRootsAndEntries();
  void rejectsMalformedAndOversizeBytes();
  void precommitFailurePreservesPriorValue();
  void writerPortUsesTheDurableBoundary();
  void postCommitDirectoryFailureIsExplicit();
  void openedFileGrowthIsBoundedBeforeReserve();
};

void FileJournalStoreTests::storesLoadsReplacesAndClears() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  FileJournalStore store(root.path());
  QCOMPARE(store.load().status, LoadStatus::Absent);

  const DisplayTransaction::Journal first = journal(QStringLiteral("first"));
  QCOMPARE(store.store(first),
           DisplayTransaction::JournalMutationOutcome::Durable);
  const QString path = root.filePath(QString::fromLatin1(kJournalName));
  struct stat metadata{};
  QVERIFY(::lstat(QFile::encodeName(path).constData(), &metadata) == 0);
  QCOMPARE(metadata.st_mode & 0777, static_cast<mode_t>(0600));
  QCOMPARE(store.load().status, LoadStatus::Loaded);
  QCOMPARE(store.load().journal, first);

  const DisplayTransaction::Journal second =
      journal(QStringLiteral("second"), QPoint(48, 0));
  QCOMPARE(store.store(second),
           DisplayTransaction::JournalMutationOutcome::Durable);
  const LoadResult loaded = store.load();
  QVERIFY(loaded.loaded());
  QCOMPARE(loaded.journal, second);
  QVERIFY(!QFile::exists(root.filePath(QString::fromLatin1(kTemporaryName))));

  QCOMPARE(store.clear(), DisplayTransaction::JournalMutationOutcome::Durable);
  QCOMPARE(store.load().status, LoadStatus::Absent);
  QCOMPARE(store.clear(), DisplayTransaction::JournalMutationOutcome::Durable);
}

void FileJournalStoreTests::interruptedTemporaryPreservesCommittedValue() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  FileJournalStore beforeCrash(root.path());
  const DisplayTransaction::Journal committed =
      journal(QStringLiteral("committed"));
  QCOMPARE(beforeCrash.store(committed),
           DisplayTransaction::JournalMutationOutcome::Durable);

  const DisplayTransaction::Journal interrupted =
      journal(QStringLiteral("interrupted"), QPoint(96, 0));
  const QByteArray interruptedBytes =
      DisplayTransaction::encodeJournal(interrupted).payload;
  QVERIFY(writeFixture(root.filePath(QString::fromLatin1(kTemporaryName)),
                       interruptedBytes.first(interruptedBytes.size() / 2)));

  FileJournalStore afterRestart(root.path());
  QCOMPARE(afterRestart.load().journal, committed);
  QCOMPARE(afterRestart.store(interrupted),
           DisplayTransaction::JournalMutationOutcome::Durable);
  QCOMPARE(afterRestart.load().journal, interrupted);
  QVERIFY(!QFile::exists(root.filePath(QString::fromLatin1(kTemporaryName))));
}

void FileJournalStoreTests::rejectsUnsafeRootsAndEntries() {
  QTemporaryDir parent;
  QVERIFY(parent.isValid());
  QVERIFY(QDir().mkdir(parent.filePath(QStringLiteral("real"))));
  const QString realRoot = parent.filePath(QStringLiteral("real"));
  const QString linkedRoot = parent.filePath(QStringLiteral("linked"));
  QVERIFY(QFile::link(realRoot, linkedRoot));
  QCOMPARE(FileJournalStore(linkedRoot).load().status, LoadStatus::Rejected);
  QCOMPARE(FileJournalStore(linkedRoot).store(journal(QStringLiteral("root"))),
           DisplayTransaction::JournalMutationOutcome::Unchanged);

  FileJournalStore store(realRoot);
  const QString journalPath =
      QDir(realRoot).filePath(QString::fromLatin1(kJournalName));
  const QString outside = parent.filePath(QStringLiteral("outside"));
  QVERIFY(writeFixture(outside, QByteArrayLiteral("outside")));
  QVERIFY(QFile::link(outside, journalPath));
  QCOMPARE(store.load().status, LoadStatus::Rejected);
  QCOMPARE(store.store(journal(QStringLiteral("symlink"))),
           DisplayTransaction::JournalMutationOutcome::Unchanged);
  QCOMPARE(store.clear(),
           DisplayTransaction::JournalMutationOutcome::Unchanged);
  QFile symlink(journalPath);
  QVERIFY(symlink.remove());
  QCOMPARE(QFile(outside).size(), qsizetype(7));

  QVERIFY(QDir().mkdir(journalPath));
  QCOMPARE(store.load().status, LoadStatus::Rejected);
  QCOMPARE(store.store(journal(QStringLiteral("directory"))),
           DisplayTransaction::JournalMutationOutcome::Unchanged);
  QCOMPARE(store.clear(),
           DisplayTransaction::JournalMutationOutcome::Unchanged);
}

void FileJournalStoreTests::rejectsMalformedAndOversizeBytes() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  FileJournalStore store(root.path());
  const QString path = root.filePath(QString::fromLatin1(kJournalName));
  QVERIFY(writeFixture(path, QByteArrayLiteral("not-a-journal")));
  QCOMPARE(store.load().status, LoadStatus::Rejected);

  QVERIFY(writeFixture(
      path, QByteArray(DisplayTransaction::kMaximumJournalBytes + 1, '\0')));
  const LoadResult oversized = store.load();
  QCOMPARE(oversized.status, LoadStatus::Rejected);
  QCOMPARE(oversized.reasonCode, QStringLiteral("journal-too-large"));

  QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner |
                                          QFileDevice::WriteOwner |
                                          QFileDevice::ReadGroup));
  QCOMPARE(store.load().reasonCode, QStringLiteral("unsafe-journal-file"));
  QCOMPARE(store.clear(),
           DisplayTransaction::JournalMutationOutcome::Unchanged);
}

void FileJournalStoreTests::precommitFailurePreservesPriorValue() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  FileJournalStore store(root.path());
  const DisplayTransaction::Journal prior = journal(QStringLiteral("prior"));
  QCOMPARE(store.store(prior),
           DisplayTransaction::JournalMutationOutcome::Durable);

  const QString temporary = root.filePath(QString::fromLatin1(kTemporaryName));
  QVERIFY(QDir().mkdir(temporary));
  QCOMPARE(store.store(journal(QStringLiteral("replacement"), QPoint(144, 0))),
           DisplayTransaction::JournalMutationOutcome::Unchanged);
  QCOMPARE(store.load().journal, prior);
}

void FileJournalStoreTests::writerPortUsesTheDurableBoundary() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  auto filesystem = std::make_unique<FileJournalStore>(root.path());
  QindaQt::DisplayWriter::WriterTransactionPort writer(
      std::make_unique<UnavailableOutputPort>(), std::move(filesystem));
  const DisplayTransaction::Journal value =
      journal(QStringLiteral("writer-port"));

  QCOMPARE(writer.storeJournal(value),
           DisplayTransaction::JournalMutationOutcome::Durable);
  QCOMPARE(FileJournalStore(root.path()).load().journal, value);
  QCOMPARE(writer.clearJournal(),
           DisplayTransaction::JournalMutationOutcome::Durable);
  QCOMPARE(FileJournalStore(root.path()).load().status, LoadStatus::Absent);
}

void FileJournalStoreTests::postCommitDirectoryFailureIsExplicit() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const DisplayTransaction::Journal prior = journal(QStringLiteral("prior"));
  QCOMPARE(FileJournalStore(root.path()).store(prior),
           DisplayTransaction::JournalMutationOutcome::Durable);

  auto hooks = std::make_shared<InjectedJournalHooks>();
  auto store = Private::FileJournalStoreTestAccess::create(root.path(), hooks);
  const DisplayTransaction::Journal replacement =
      journal(QStringLiteral("replacement"), QPoint(192, 0));
  hooks->nextDirectorySync = false;
  QCOMPARE(store->store(replacement),
           DisplayTransaction::JournalMutationOutcome::DurabilityUncertain);
  QCOMPARE(FileJournalStore(root.path()).load().journal, replacement);

  hooks->nextDirectorySync = false;
  QCOMPARE(store->clear(),
           DisplayTransaction::JournalMutationOutcome::DurabilityUncertain);
  QCOMPARE(FileJournalStore(root.path()).load().status, LoadStatus::Absent);
}

void FileJournalStoreTests::openedFileGrowthIsBoundedBeforeReserve() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const DisplayTransaction::Journal value = journal(QStringLiteral("growth"));
  FileJournalStore writer(root.path());
  QCOMPARE(writer.store(value),
           DisplayTransaction::JournalMutationOutcome::Durable);

  auto hooks = std::make_shared<InjectedJournalHooks>();
  hooks->growPath = root.filePath(QString::fromLatin1(kJournalName));
  auto reader = Private::FileJournalStoreTestAccess::create(root.path(), hooks);
  const LoadResult loaded = reader->load();
  QVERIFY(hooks->growthAttempted);
  QVERIFY(hooks->growthSucceeded);
  QCOMPARE(loaded.status, LoadStatus::Rejected);
  QCOMPARE(loaded.reasonCode, QStringLiteral("journal-too-large"));
}

QTEST_GUILESS_MAIN(FileJournalStoreTests)
#include "tst_file_journal_store.moc"
