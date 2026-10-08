// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_test_support.h"
#include "mutation/safe_tree_operations.h"
#include <QtTest>
#include <fcntl.h>
#include <unistd.h>
using namespace RecoveryTest;
class RecoveryManifestTests final : public QObject {
  Q_OBJECT
private slots:
  void independentContentAndIdentityDigests() {
    RecoveryManifest v;
    RecoveryManifestEntry e; e.identity = {1, 2, 3, 4, S_IFREG | 0600};
    e.mountId = 5; e.contentDigest = QByteArray(32, 'a'); v.entries = {e}; v.regularBytes = 3;
    const auto content = recoveryManifestDigest(v, false), identity = recoveryManifestDigest(v, true);
    QVERIFY(!content.isEmpty()); QVERIFY(!identity.isEmpty());
    auto copied = v; copied.entries[0].identity.device = 8; copied.entries[0].identity.inode = 9;
    copied.entries[0].mountId = 10;
    QCOMPARE(recoveryManifestDigest(copied, false), content);
    QVERIFY(recoveryManifestDigest(copied, true) != identity);
    QVERIFY(!sameRecoveryTree(v, copied));
    copied = v; ++copied.entries[0].changedNanoseconds;
    QVERIFY(!sameRecoveryTree(v, copied)); QVERIFY(sameRecoveryTree(v, copied, true));
    copied.entries[0].contentDigest.fill('b'); QVERIFY(!sameRecoveryTree(v, copied, true));
    copied = v; copied.entries[0].relativePath = "bad"; QVERIFY(recoveryManifestDigest(copied, false).isEmpty());
    copied = v; copied.entries[0].identity.mode = S_IFLNK | 0777; QVERIFY(recoveryManifestDigest(copied, true).isEmpty());
    copied = v; copied.regularBytes = 4; QVERIFY(recoveryManifestDigest(copied, true).isEmpty());
  }
  void realReadbackAndChildLateWrite() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("tree")));
    const auto child = QDir(directory.path()).filePath(QStringLiteral("tree/child"));
    QVERIFY(write(child, "original"));
    MutationResult result;
    auto parent = RecoveryDirectoryAdmission::open(directory.path(), result); QVERIFY(parent);
    const auto first = captureRecoveryManifest(*parent, "tree"); QVERIFY(first.result.ok()); QVERIFY(first.manifest);
    QVERIFY(write(child, "late-byte-change"));
    const auto second = captureRecoveryManifest(*parent, "tree"); QVERIFY(second.result.ok()); QVERIFY(second.manifest);
    QVERIFY(!sameRecoveryTree(*first.manifest, *second.manifest));
  }
  void hostileKindsAndNestedMount() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const auto target = QDir(directory.path()).filePath(QStringLiteral("file"));
    QVERIFY(write(target, "original"));
    QVERIFY(::symlink("file", QFile::encodeName(QDir(directory.path()).filePath(QStringLiteral("link"))).constData()) == 0);
    QVERIFY(::mkfifo(QFile::encodeName(QDir(directory.path()).filePath(QStringLiteral("fifo"))).constData(), 0600) == 0);
    MutationResult result;
    auto parent = RecoveryDirectoryAdmission::open(directory.path(), result); QVERIFY(parent);
    QCOMPARE(captureRecoveryManifest(*parent, "link").result.error, MutationError::SymlinkEscape);
    QCOMPARE(captureRecoveryManifest(*parent, "fifo").result.error, MutationError::Unsupported);
    auto device = RecoveryDirectoryAdmission::open(QStringLiteral("/dev"), result); QVERIFY(device);
    // Reject /dev/shm's distinct mount before enumerating any of its contents.
    QCOMPARE(captureRecoveryManifest(*device, "shm").result.error, MutationError::Unsupported);
  }
  void strictCopyObservedSizeBound_data() {
    QTest::addColumn<QString>("change");
    for (const auto &name : {"grow", "truncate", "exact", "empty"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }
  void strictCopyObservedSizeBound() {
    QFETCH(QString, change);
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const auto sourcePath = directory.filePath(QStringLiteral("source"));
    const auto outputPath = directory.filePath(QStringLiteral("output"));
    const QByteArray initial(change == QStringLiteral("empty") ? 0 : 64 * 1024, 'a');
    QVERIFY(write(sourcePath, initial));
    MutationResult admission;
    auto parent = RecoveryDirectoryAdmission::open(directory.path(), admission); QVERIFY(parent);
    bool modified = false, mutationOk = true;
    const auto result = copyRecoveryTreeAt(parent->descriptor(), "source", parent->descriptor(), "output", {},
        [&](const MutationProgress &) {
          if (modified || (change != QStringLiteral("grow") && change != QStringLiteral("truncate"))) return;
          modified = true;
          QFile held(sourcePath);
          mutationOk = held.open(QIODevice::WriteOnly | QIODevice::Append);
          if (mutationOk) mutationOk = change == QStringLiteral("grow")
              ? held.write(QByteArray(64 * 1024, 'b')) == 64 * 1024 : held.resize(0);
        }, 20000);
    QVERIFY(mutationOk);
    QVERIFY(QFileInfo(outputPath).size() <= initial.size());
    if (change == QStringLiteral("grow") || change == QStringLiteral("truncate")) {
      QVERIFY(modified); QCOMPARE(result.error, MutationError::Changed);
      QCOMPARE(QFileInfo(sourcePath).size(), change == QStringLiteral("grow") ? qint64(128 * 1024) : qint64(0));
    } else {
      QVERIFY(result.ok()); QCOMPARE(read(outputPath), initial); QCOMPARE(read(sourcePath), initial);
    }
  }
  void cancellationAndEntryBound() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    QVERIFY(write(QDir(directory.path()).filePath(QStringLiteral("file")), "bytes"));
    MutationResult result;
    auto parent = RecoveryDirectoryAdmission::open(directory.path(), result); QVERIFY(parent);
    const auto cancel = std::make_shared<std::atomic_bool>(true);
    QCOMPARE(captureRecoveryManifest(*parent, "file", cancel).result.error, MutationError::Cancelled);
    RecoveryManifest v;
    RecoveryManifestEntry root; root.identity = {1, 2, 0, 0, S_IFDIR | 0700}; root.mountId = 3;
    v.entries = {root};
    for (int i = 0; i < 20000; ++i) {
      auto entry = root; entry.relativePath = QByteArray::number(i).rightJustified(5, '0');
      v.entries.append(entry);
    }
    QVERIFY(recoveryManifestDigest(v, true).isEmpty());
  }
};
QTEST_GUILESS_MAIN(RecoveryManifestTests)
#include "tst_recovery_manifest.moc"
