// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>
#include <QTest>

#include "job_fakes.h"
#include "link_resolution.h"
#include "staged_tree_check.h"

#include <sys/stat.h>
#include <unistd.h>

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

// The second path-escape gate: the REAL staged tree, links resolved
// physically (the reviewer's lexical_escape case), hard links counted
// against st_nlink, special files refused. Trees are built by hand.
namespace {

const QString kTool = QStringLiteral("GE-Proton99-1-x86_64");

struct Tree {
  QTemporaryDir dir{QDir::homePath() + QStringLiteral("/staged-XXXXXX")};
  QString extract = dir.filePath(QStringLiteral("staging/extract"));
  QString top = extract + QLatin1Char('/') + kTool;

  Tree() { writeFile(top + QStringLiteral("/proton"), 16); }

  bool link(const QString &target, const QString &relative) const {
    QDir().mkpath(QFileInfo(top + QLatin1Char('/') + relative).absolutePath());
    return ::symlink(QFile::encodeName(target).constData(),
                     QFile::encodeName(top + QLatin1Char('/') + relative).constData()) == 0;
  }

  StagedTreeVerdict verify() const { return verifyStagedBuild(extract, kTool); }
};

LinkReader tableReader(const QHash<QString, QString> &links) {
  return [links](const QStringList &path) -> std::optional<QString> {
    const auto it = links.constFind(path.join(QLatin1Char('/')));
    return it == links.constEnd() ? std::nullopt : std::optional<QString>(it.value());
  };
}

} // namespace

class tst_staged_tree_check : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void benignGeLikeTreePasses() {
    Tree t;
    writeFile(t.top + QStringLiteral("/files/lib/wine/x86_64-unix/ntdll.so"), 64);
    writeFile(t.top + QStringLiteral("/files/share/default_pfx/drive_c/windows/win.ini"), 8);
    QVERIFY(t.link(QStringLiteral("files/lib"), QStringLiteral("lib")));
    QVERIFY(t.link(QStringLiteral("../../lib/wine/x86_64-unix/ntdll.so"),
                   QStringLiteral("files/share/ntdll-link.so")));
    QVERIFY(t.link(QStringLiteral("../drive_c"), QStringLiteral("files/share/default_pfx/dosdevices/c:")));
    QVERIFY(t.link(QStringLiteral("../lib/wine"), QStringLiteral("lib-alias/wine"))); // via a chain
    QVERIFY(t.link(QStringLiteral("missing-inside"), QStringLiteral("dangling")));
    QCOMPARE(::link(QFile::encodeName(t.top + QStringLiteral("/proton")).constData(),
                    QFile::encodeName(t.top + QStringLiteral("/proton-copy")).constData()),
             0);
    const auto verdict = t.verify();
    QVERIFY2(verdict.ok, qPrintable(verdict.reason));
    QVERIFY(verdict.entries > 8);
  }

  void reviewerLexicalEscapeIsRefused() {
    Tree t;
    QDir().mkpath(t.top + QStringLiteral("/a/b/c/d/e2"));
    QVERIFY(t.link(QStringLiteral("../../../../.."), QStringLiteral("a/b/c/d/e2/s")));
    QVERIFY(t.link(QStringLiteral("s/../../../outside"), QStringLiteral("a/b/c/d/e2/esc")));
    const auto verdict = t.verify();
    QVERIFY(!verdict.ok);
    QVERIFY2(verdict.reason.contains(QStringLiteral("esc")), qPrintable(verdict.reason));
    // The link really does lead outside on disk -- the check is not paranoid.
    const QString real = QFileInfo(t.top + QStringLiteral("/a/b/c/d/e2/esc")).canonicalFilePath();
    QVERIFY(real.isEmpty() || !real.startsWith(t.top));
  }

  void escapes_data() {
    QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("name");
    QTest::newRow("absolute") << QStringLiteral("/etc") << QStringLiteral("abs");
    QTest::newRow("plain climb") << QStringLiteral("../../outside") << QStringLiteral("files/up");
    QTest::newRow("to parent of top") << QStringLiteral("..") << QStringLiteral("parent");
  }
  void escapes() {
    QFETCH(QString, target);
    QFETCH(QString, name);
    Tree t;
    QVERIFY(t.link(target, name));
    QVERIFY(!t.verify().ok);
  }

  void linkLoopIsRefused() {
    Tree t;
    QVERIFY(t.link(QStringLiteral("y"), QStringLiteral("x")));
    QVERIFY(t.link(QStringLiteral("x/z"), QStringLiteral("y")));
    QVERIFY(!t.verify().ok);
  }

  void hardLinkToAFileOutsideIsRefused() {
    Tree t;
    writeFile(t.dir.filePath(QStringLiteral("secret")), 4);
    QCOMPARE(::link(QFile::encodeName(t.dir.filePath(QStringLiteral("secret"))).constData(),
                    QFile::encodeName(t.top + QStringLiteral("/stolen")).constData()),
             0);
    const auto verdict = t.verify();
    QVERIFY(!verdict.ok);
    QVERIFY(verdict.reason.contains(QStringLiteral("hard link")));
  }

  void specialFilesAndSetuidAreRefused() {
    Tree fifo;
    QCOMPARE(::mkfifo(QFile::encodeName(fifo.top + QStringLiteral("/pipe")).constData(), 0600), 0);
    QVERIFY(fifo.verify().reason.contains(QStringLiteral("special file")));
    Tree setuid;
    QCOMPARE(::chmod(QFile::encodeName(setuid.top + QStringLiteral("/proton")).constData(), 04755), 0);
    QVERIFY(setuid.verify().reason.contains(QStringLiteral("setuid")));
  }

  void topThatIsALinkIsRefused() {
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/staged-XXXXXX"));
    const QString extract = dir.filePath(QStringLiteral("extract"));
    QDir().mkpath(extract);
    QVERIFY(QFile::link(dir.path(), extract + QLatin1Char('/') + kTool));
    QVERIFY(!verifyStagedBuild(extract, kTool).ok);
  }

  void entryCountIsBounded() {
    Tree t;
    for (int i = 0; i < 20; ++i) {
      writeFile(t.top + QStringLiteral("/f%1").arg(i), 1);
    }
    QVERIFY(!verifyStagedBuild(t.extract, kTool, 10).ok);
  }

  void resolverFollowsLinksPhysically() {
    const QHash<QString, QString> links{{QStringLiteral("T/a/s"), QStringLiteral("..")},
                                        {QStringLiteral("T/lib"), QStringLiteral("files/lib")}};
    const LinkReader reader = tableReader(links);
    // Lexically "a/s/../x" is T/a/x; physically s is T, so it is T/../x.
    QVERIFY(!resolveConfined({QStringLiteral("T"), QStringLiteral("a")}, QStringLiteral("s/../x"), reader));
    QCOMPARE(resolveConfined({QStringLiteral("T"), QStringLiteral("a")}, QStringLiteral("s/lib/wine"), reader)
                 .value(),
             QStringList({QStringLiteral("T"), QStringLiteral("files"), QStringLiteral("lib"),
                          QStringLiteral("wine")}));
    QVERIFY(!resolveConfined({QStringLiteral("T")}, QStringLiteral("/abs"), reader));
    QCOMPARE(resolvePathConfined({QStringLiteral("T"), QStringLiteral("lib"), QStringLiteral("x")}, reader)
                 .value(),
             QStringList({QStringLiteral("T"), QStringLiteral("files"), QStringLiteral("lib"),
                          QStringLiteral("x")}));
  }
};

QTEST_GUILESS_MAIN(tst_staged_tree_check)
#include "tst_staged_tree_check.moc"
