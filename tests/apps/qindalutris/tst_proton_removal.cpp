// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "job_fakes.h"
#include "job_log.h"
#include "proton_removal.h"

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

namespace {

const QString kBuild = QStringLiteral("GE-Proton11-7-x86_64");

struct Fixture {
  QTemporaryDir dir{QDir::homePath() + QStringLiteral("/removal-XXXXXX")};
  QString root = dir.filePath(QStringLiteral("compatibilitytools.d"));

  Fixture() {
    makeProtonBuild(root, kBuild);
    writeFile(root + QLatin1Char('/') + kBuild + QStringLiteral("/files/lib/wine.so"), 128);
  }

  ProtonRemovalRequest request(const QString &name = kBuild) const {
    ProtonRemovalRequest r;
    r.userRoot = root;
    r.buildName = name;
    return r;
  }

  QStringList entries() const {
    return QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
  }
};

} // namespace

class tst_proton_removal : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void unpinnedUserBuildIsRemoved() {
    Fixture f;
    JobLog log;
    const ProtonRemovalResult result = removeProtonBuild(f.request(), &log);
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.message, kBuild + QStringLiteral(" was removed."));
    QVERIFY(!QFileInfo::exists(f.root + QLatin1Char('/') + kBuild));
    // The trash directory may remain, but it is empty.
    for (const QString &entry : f.entries()) {
      QCOMPARE(entry, QStringLiteral(".qindalutris-trash"));
      QVERIFY(QDir(f.root + QStringLiteral("/.qindalutris-trash"))
                  .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
                  .isEmpty());
    }
    QVERIFY(log.text().contains(QStringLiteral("trash")));
  }

  void pinnedBuildIsRefused() {
    Fixture f;
    ProtonRemovalRequest request = f.request();
    request.pinnedBuildNames = {QStringLiteral("GE-Proton11-6-x86_64"), kBuild};
    const ProtonRemovalResult result = removeProtonBuild(request);
    QVERIFY(!result.ok);
    QVERIFY(result.message.contains(QStringLiteral("still used by at least one of your games")));
    QVERIFY(QFileInfo(f.root + QLatin1Char('/') + kBuild + QStringLiteral("/proton")).isFile());
  }

  void pinOfAnotherBuildDoesNotBlock() {
    Fixture f;
    ProtonRemovalRequest request = f.request();
    request.pinnedBuildNames = {QStringLiteral("GE-Proton11-6-x86_64")};
    QVERIFY(removeProtonBuild(request).ok);
  }

  void systemRootsAreRefused_data() {
    QTest::addColumn<QString>("root");
    QTest::newRow("portage root") << QStringLiteral("/usr/share/steam/compatibilitytools.d");
    QTest::newRow("opt") << QStringLiteral("/opt/steam/compatibilitytools.d");
    QTest::newRow("filesystem root") << QStringLiteral("/");
  }
  void systemRootsAreRefused() {
    QFETCH(QString, root);
    ProtonRemovalRequest request;
    request.userRoot = root;
    request.buildName = kBuild;
    const auto refused = checkProtonRemoval(request);
    QVERIFY(refused.has_value());
    QVERIFY2(refused->contains(QStringLiteral("cannot remove")), qPrintable(*refused));
  }

  void injectedSystemRootProtectsARealDirectory() {
    Fixture f;
    ProtonRemovalRequest request = f.request();
    request.systemRoots = {f.dir.path()};
    const ProtonRemovalResult result = removeProtonBuild(request);
    QVERIFY(!result.ok);
    QVERIFY(result.message.contains(QStringLiteral("package manager")));
    QVERIFY(QFileInfo::exists(f.root + QLatin1Char('/') + kBuild));
  }

  void symlinkedBuildIsNotFollowed() {
    Fixture f;
    const QString elsewhere = f.dir.filePath(QStringLiteral("elsewhere"));
    makeProtonBuild(elsewhere, QStringLiteral("Real"));
    QVERIFY(QFile::link(elsewhere + QStringLiteral("/Real"), f.root + QStringLiteral("/Linked")));
    const ProtonRemovalResult result = removeProtonBuild(f.request(QStringLiteral("Linked")));
    QVERIFY(!result.ok);
    QVERIFY(QFileInfo(elsewhere + QStringLiteral("/Real/proton")).isFile());
  }

  void missingAndUnsafeNamesAreRefused() {
    Fixture f;
    QCOMPARE(removeProtonBuild(f.request(QStringLiteral("GE-Proton1-1"))).message,
             QStringLiteral("GE-Proton1-1 is not installed."));
    QVERIFY(!removeProtonBuild(f.request(QStringLiteral("../compatibilitytools.d"))).ok);
    QVERIFY(!removeProtonBuild(f.request(QStringLiteral(".."))).ok);
    QVERIFY(!removeProtonBuild(f.request(QString())).ok);
    ProtonRemovalRequest relative = f.request();
    relative.userRoot = QStringLiteral("compatibilitytools.d");
    QVERIFY(!removeProtonBuild(relative).ok);
    QVERIFY(QFileInfo::exists(f.root + QLatin1Char('/') + kBuild));
  }

  void sweepClearsInterruptedRemovals() {
    Fixture f;
    writeFile(f.root + QStringLiteral("/.qindalutris-trash/Old-1234/file"), 4);
    sweepProtonTrash(f.root);
    QVERIFY(!QFileInfo::exists(f.root + QStringLiteral("/.qindalutris-trash")));
    QVERIFY(QFileInfo::exists(f.root + QLatin1Char('/') + kBuild));
  }
};

QTEST_GUILESS_MAIN(tst_proton_removal)
#include "tst_proton_removal.moc"
