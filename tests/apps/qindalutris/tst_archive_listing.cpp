// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "archive_listing.h"

using namespace QindaQt::QindaLutris;

// The ADR-0275 archive gate over `tar --list --verbose --numeric-owner
// --quoting-style=c` output. Lines are written exactly as GNU tar 1.35
// prints them (see the reviewer's fixtures for abs/rel symlinks, hard links
// with '..', devices, and the symlink-then-directory overwrite).
namespace {

const QByteArray kTop = "GE-Proton99-1-x86_64";

QByteArray dir(const QByteArray &name, const QByteArray &mode = "rwxr-xr-x") {
  return "d" + mode + " 0/0               0 2026-09-25 00:00 \"" + name + "/\"\n";
}
QByteArray file(const QByteArray &name, const QByteArray &mode = "rw-r--r--") {
  return "-" + mode + " 0/0              10 2026-09-25 00:00 \"" + name + "\"\n";
}
QByteArray symlinkLine(const QByteArray &name, const QByteArray &target) {
  return "lrwxrwxrwx 0/0               0 2026-09-25 00:00 \"" + name + "\" -> \"" + target + "\"\n";
}
QByteArray hardlinkLine(const QByteArray &name, const QByteArray &target) {
  return "hrw-r--r-- 0/0               0 2026-09-25 00:00 \"" + name + "\" link to \"" + target + "\"\n";
}
QByteArray base() { return dir(kTop) + file(kTop + "/proton", "rwxr-xr-x"); }

} // namespace

class tst_archive_listing : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void realisticBuildIsAccepted() {
    const QByteArray listing = base() + dir(kTop + "/files") + dir(kTop + "/files/lib") +
                               file(kTop + "/files/lib/libwine.so.1.0") +
                               symlinkLine(kTop + "/files/lib/libwine.so.1", "libwine.so.1.0") +
                               symlinkLine(kTop + "/files/share/default_pfx/dosdevices/c:", "../drive_c") +
                               hardlinkLine(kTop + "/files/lib/copy.so", kTop + "/files/lib/libwine.so.1.0") +
                               file(kTop + "/files/name with \\\" -> quote");
    const auto verdict = validateArchiveListing(listing);
    QVERIFY2(verdict.ok, qPrintable(verdict.reason));
    QCOMPARE(verdict.topLevel, QString::fromLatin1(kTop));
  }

  void cQuotingIsDecoded() {
    const auto entry = parseArchiveListingLine(
        "lrwxrwxrwx 1000/1000 0 2026-09-25 16:16 \"a b/l -> k\" -> \"x -> y\"");
    QVERIFY(entry.has_value());
    QCOMPARE(entry->type, QLatin1Char('l'));
    QCOMPARE(entry->name, QStringLiteral("a b/l -> k"));
    QCOMPARE(entry->linkTarget, QStringLiteral("x -> y"));
    const auto escaped = parseArchiveListingLine("-rw-r--r-- 0/0 1 2026-09-25 16:16 \"a b/new\\nline\\303\\251\"");
    QVERIFY(escaped.has_value());
    QCOMPARE(escaped->name, QStringLiteral("a b/new\nlineé"));
    QVERIFY(!parseArchiveListingLine("-rw-r--r-- 0/0 1 2026-09-25 16:16 \"unterminated").has_value());
    QVERIFY(!parseArchiveListingLine("-rw-r--r-- 0/0 1 2026-09-25 16:16 \"a\" trailing").has_value());
  }

  void refusals_data() {
    QTest::addColumn<QByteArray>("listing");
    QTest::addColumn<QByteArray>("stderrText");
    QTest::addColumn<QString>("reason");
    QTest::newRow("absolute name") << base() + file("/etc/passwd") << QByteArray() << "absolute";
    QTest::newRow("dotdot name") << base() + file("../escape.txt") << QByteArray() << "outside its folder";
    QTest::newRow("inner dotdot") << base() + file(kTop + "/../../x") << QByteArray() << "outside its folder";
    QTest::newRow("second top") << base() + file("other/x") << QByteArray() << "more than one";
    QTest::newRow("top is a symlink")
        << symlinkLine(kTop, "/tmp/outside") + file(kTop + "/pwned") << QByteArray() << "not a folder";
    QTest::newRow("absolute symlink")
        << base() + symlinkLine(kTop + "/lnk", "/tmp/outside") << QByteArray() << "pointing outside";
    QTest::newRow("relative symlink escaping")
        << base() + symlinkLine(kTop + "/lnk", "../../../../tmp/outside") << QByteArray() << "pointing outside";
    QTest::newRow("symlink to parent of top")
        << base() + symlinkLine(kTop + "/up", "..") << QByteArray() << "pointing outside";
    QTest::newRow("lexically inside, physically outside (reviewer)")
        << base() + dir(kTop + "/a") + dir(kTop + "/a/b") + dir(kTop + "/a/b/c") + dir(kTop + "/a/b/c/d") +
               dir(kTop + "/a/b/c/d/e2") + symlinkLine(kTop + "/a/b/c/d/e2/s", "../../../../..") +
               symlinkLine(kTop + "/a/b/c/d/e2/esc", "s/../../../outside")
        << QByteArray() << "pointing outside";
    QTest::newRow("link loop") << base() + symlinkLine(kTop + "/x", "y") + symlinkLine(kTop + "/y", "x/z")
                               << QByteArray() << "pointing outside";
    QTest::newRow("hardlink through a symlink")
        << base() + dir(kTop + "/files") + symlinkLine(kTop + "/l", "files") +
               hardlinkLine(kTop + "/h", kTop + "/l/proton")
        << QByteArray() << "through a link";
    QTest::newRow("hardlink outside")
        << base() + hardlinkLine(kTop + "/h", "tmp/claude-1000/review-jobs/outside/secret") << QByteArray()
        << "hard link";
    QTest::newRow("hardlink dotdot") << base() + hardlinkLine(kTop + "/h", kTop + "/../../x") << QByteArray()
                                     << "hard link";
    QTest::newRow("char device")
        << base() + "crw-r--r-- 0/0             1,3 2026-09-25 00:00 \"" + kTop + "/dev\"\n" << QByteArray()
        << "special file";
    QTest::newRow("fifo") << base() + "prw-r--r-- 0/0 0 2026-09-25 00:00 \"" + kTop + "/fifo\"\n"
                          << QByteArray() << "special file";
    QTest::newRow("setuid") << base() + file(kTop + "/suid", "rwsr-xr-x") << QByteArray() << "setuid";
    QTest::newRow("symlink then directory")
        << base() + symlinkLine(kTop + "/d", "/tmp/outside") + dir(kTop + "/d") + file(kTop + "/d/pwned")
        << QByteArray() << "pointing outside";
    QTest::newRow("inside symlink then file below it")
        << base() + symlinkLine(kTop + "/d", "files") + file(kTop + "/d/pwned") << QByteArray() << "through a link";
    QTest::newRow("duplicate") << base() + file(kTop + "/a") + file(kTop + "/a") << QByteArray() << "more than once";
    QTest::newRow("tar rewrote names") << base() << QByteArray("tar: Removing leading `../' from member names")
                                       << "rewrite";
    QTest::newRow("single file") << file("proton") << QByteArray() << "not a folder";
    QTest::newRow("empty") << QByteArray() << QByteArray() << "single folder";
    QTest::newRow("garbage") << base() + "not a listing line\n" << QByteArray() << "Unreadable";
  }
  void refusals() {
    QFETCH(QByteArray, listing);
    QFETCH(QByteArray, stderrText);
    QFETCH(QString, reason);
    const auto verdict = validateArchiveListing(listing, stderrText);
    QVERIFY(!verdict.ok);
    QVERIFY2(verdict.reason.contains(reason), qPrintable(verdict.reason));
    QVERIFY(verdict.topLevel.isEmpty());
  }

  void readOnlyFolderIsAllowed() {
    // A 0555 folder is legitimate content; the removal code copes with it.
    QVERIFY(validateArchiveListing(base() + dir(kTop + "/ro", "r-xr-xr-x") + file(kTop + "/ro/f")).ok);
  }

  void entryCountIsBounded() {
    QByteArray listing = base();
    for (int i = 0; i < 10; ++i) {
      listing += file(kTop + "/f" + QByteArray::number(i));
    }
    QVERIFY(!validateArchiveListing(listing, {}, 5).ok);
    QVERIFY(validateArchiveListing(listing, {}, 50).ok);
  }
};

QTEST_GUILESS_MAIN(tst_archive_listing)
#include "tst_archive_listing.moc"
