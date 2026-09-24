// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/reveal_request.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool writeFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write("payload") == 7;
}

[[nodiscard]] QString uriOf(const QString &path) {
  return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
}

[[nodiscard]] QString canonical(const QString &path) {
  return QFileInfo(path).canonicalFilePath();
}

} // namespace

// ADR-0273: the URI and path policy every org.freedesktop.FileManager1 call
// passes before any window is shown.
class TestRevealRequest final : public QObject {
  Q_OBJECT

private slots:
  void foldersResolveOnceToCanonicalDirectories();
  void itemsGroupByFolderInFirstSeenOrder();
  void itemPropertiesAskForTheDialogAndRootIsAFolder();
  void symbolicLinksAreEntriesEvenWhenDangling();
  void refusesEverythingThatIsNotALocalFileUri();
  void refusesMissingNonDirectoryAndUnreadableTargets();
  void oneBadUriRefusesTheWholeCall();
  void boundsUrisAndWindows();
  void namesMustBeSingleEntries();
  void onlyTheDocumentedActionsMayFollowAReveal();
};

void TestRevealRequest::foldersResolveOnceToCanonicalDirectories() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString unusual = QStringLiteral("Tést folder #1 %20");
  QVERIFY(QDir(root.path()).mkdir(unusual));
  QVERIFY(QFile::link(root.filePath(unusual), root.filePath(QStringLiteral("Link"))));
  const QString folder = canonical(root.filePath(unusual));

  // The folder, a link to it and a localhost URI of it are one window.
  const QString localhost = QStringLiteral("file://localhost") +
                            QUrl::fromLocalFile(folder).path(QUrl::FullyEncoded);
  const RevealPlan plan =
      planReveal(RevealKind::Folders, {uriOf(root.filePath(unusual)),
                                       uriOf(root.filePath(QStringLiteral("Link"))), localhost});
  QVERIFY2(plan.ok(), qPrintable(plan.diagnostic));
  QCOMPARE(plan.requests, (QList<RevealRequest>{{folder, {}, {}}}));

  // Nothing to show is not an error.
  const RevealPlan empty = planReveal(RevealKind::Items, {});
  QVERIFY(empty.ok());
  QVERIFY(empty.requests.isEmpty());
}

void TestRevealRequest::itemsGroupByFolderInFirstSeenOrder() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(QDir(root.path()).mkpath(QStringLiteral("A/Sub")));
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("B")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("A/one.txt"))));
  QVERIFY(writeFile(root.filePath(QStringLiteral("A/two.txt"))));
  QVERIFY(writeFile(root.filePath(QStringLiteral("B/three.txt"))));
  const QString a = canonical(root.filePath(QStringLiteral("A")));
  const QString b = canonical(root.filePath(QStringLiteral("B")));

  const RevealPlan plan = planReveal(
      RevealKind::Items,
      {uriOf(root.filePath(QStringLiteral("A/two.txt"))),
       uriOf(root.filePath(QStringLiteral("B/three.txt"))),
       uriOf(root.filePath(QStringLiteral("A/one.txt"))),
       uriOf(root.filePath(QStringLiteral("A/two.txt"))),
       // A folder is an item of its parent; a trailing slash changes nothing.
       uriOf(root.filePath(QStringLiteral("A/Sub"))) + QStringLiteral("/"),
       // ".." resolves as the kernel does, not lexically.
       uriOf(root.filePath(QStringLiteral("B/../A/one.txt")))});
  QVERIFY2(plan.ok(), qPrintable(plan.diagnostic));
  QCOMPARE(plan.requests,
           (QList<RevealRequest>{
               {a, {QStringLiteral("two.txt"), QStringLiteral("one.txt"), QStringLiteral("Sub")},
                {}},
               {b, {QStringLiteral("three.txt")}, {}}}));
}

void TestRevealRequest::itemPropertiesAskForTheDialogAndRootIsAFolder() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(writeFile(root.filePath(QStringLiteral("notes.txt"))));
  const RevealPlan properties = planReveal(
      RevealKind::ItemProperties, {uriOf(root.filePath(QStringLiteral("notes.txt")))});
  QVERIFY2(properties.ok(), qPrintable(properties.diagnostic));
  QCOMPARE(properties.requests,
           (QList<RevealRequest>{{canonical(root.path()), {QStringLiteral("notes.txt")},
                                  QStringLiteral("file.properties")}}));

  // "/" has no folder to be selected in, so it is shown as a folder.
  const RevealPlan rootItem = planReveal(RevealKind::Items, {QStringLiteral("file:///")});
  QVERIFY2(rootItem.ok(), qPrintable(rootItem.diagnostic));
  QCOMPARE(rootItem.requests, (QList<RevealRequest>{{QStringLiteral("/"), {}, {}}}));
}

void TestRevealRequest::symbolicLinksAreEntriesEvenWhenDangling() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("Elsewhere")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("Elsewhere/target.txt"))));
  QVERIFY(QFile::link(root.filePath(QStringLiteral("Elsewhere/target.txt")),
                      root.filePath(QStringLiteral("link.txt"))));
  QVERIFY(QFile::link(root.filePath(QStringLiteral("absent")),
                      root.filePath(QStringLiteral("dangling"))));

  // The link is selected in its own folder, never its target's.
  const RevealPlan plan =
      planReveal(RevealKind::Items, {uriOf(root.filePath(QStringLiteral("link.txt"))),
                                     uriOf(root.filePath(QStringLiteral("dangling")))});
  QVERIFY2(plan.ok(), qPrintable(plan.diagnostic));
  QCOMPARE(plan.requests,
           (QList<RevealRequest>{{canonical(root.path()),
                                  {QStringLiteral("link.txt"), QStringLiteral("dangling")},
                                  {}}}));
}

void TestRevealRequest::refusesEverythingThatIsNotALocalFileUri() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString path = QUrl::fromLocalFile(root.path()).path(QUrl::FullyEncoded);
  const QStringList hostile{
      QStringLiteral("https://example.com") + path,
      QStringLiteral("smb://server/share"),
      QStringLiteral("sftp://host") + path,
      QStringLiteral("file://otherhost") + path,
      QStringLiteral("file://user@localhost") + path,
      QStringLiteral("file://localhost:8080") + path,
      QStringLiteral("file://") + path + QStringLiteral("?query=1"),
      QStringLiteral("file://") + path + QStringLiteral("#fragment"),
      // A bare path is not a URI; relative forms name nothing.
      root.path(),
      QStringLiteral("relative/folder"),
      QStringLiteral("file:relative"),
      QString(),
  };
  for (const QString &uri : hostile) {
    for (const RevealKind kind :
         {RevealKind::Folders, RevealKind::Items, RevealKind::ItemProperties}) {
      const RevealPlan plan = planReveal(kind, {uri});
      QVERIFY2(!plan.ok(), qPrintable(uri));
      QCOMPARE(plan.error, RevealError::NotLocal);
      QVERIFY(!plan.diagnostic.isEmpty());
      QVERIFY(plan.requests.isEmpty());
    }
  }
  // An encoded NUL never reaches a stat call as a shortened path.
  const RevealPlan nul =
      planReveal(RevealKind::Folders, {QStringLiteral("file://") + path + QStringLiteral("%00x")});
  QVERIFY(!nul.ok());
  QVERIFY(nul.requests.isEmpty());
}

void TestRevealRequest::refusesMissingNonDirectoryAndUnreadableTargets() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(writeFile(root.filePath(QStringLiteral("notes.txt"))));
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("Locked")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("Locked/inside.txt"))));
  const auto plan = [&root](RevealKind kind, const QString &relative) {
    return planReveal(kind, {uriOf(root.filePath(relative))});
  };
  QCOMPARE(plan(RevealKind::Folders, QStringLiteral("missing")).error, RevealError::NotFound);
  QCOMPARE(plan(RevealKind::Folders, QStringLiteral("notes.txt")).error,
           RevealError::NotDirectory);
  QCOMPARE(plan(RevealKind::Items, QStringLiteral("missing.txt")).error, RevealError::NotFound);
  QCOMPARE(plan(RevealKind::Items, QStringLiteral("missing/notes.txt")).error,
           RevealError::NotFound);
  // A file is not a folder to select an entry in.
  QCOMPARE(plan(RevealKind::Items, QStringLiteral("notes.txt/child")).error,
           RevealError::NotDirectory);

  QFile locked(root.filePath(QStringLiteral("Locked")));
  QVERIFY(locked.setPermissions(QFileDevice::WriteOwner));
  if (QFileInfo(root.filePath(QStringLiteral("Locked"))).isReadable()) {
    QVERIFY(locked.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                  QFileDevice::ExeOwner));
    QSKIP("running with privileges that ignore directory permissions");
  }
  QCOMPARE(plan(RevealKind::Folders, QStringLiteral("Locked")).error, RevealError::Unreadable);
  QCOMPARE(plan(RevealKind::Items, QStringLiteral("Locked/inside.txt")).error,
           RevealError::Unreadable);
  QVERIFY(locked.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                QFileDevice::ExeOwner));
}

void TestRevealRequest::oneBadUriRefusesTheWholeCall() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(writeFile(root.filePath(QStringLiteral("notes.txt"))));
  const RevealPlan plan =
      planReveal(RevealKind::Items, {uriOf(root.filePath(QStringLiteral("notes.txt"))),
                                     uriOf(root.filePath(QStringLiteral("gone.txt")))});
  QCOMPARE(plan.error, RevealError::NotFound);
  QVERIFY(plan.requests.isEmpty());
}

void TestRevealRequest::boundsUrisAndWindows() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QStringList folders;
  for (qsizetype index = 0; index <= maximumRevealWindows; ++index) {
    const QString name = QStringLiteral("F%1").arg(index);
    QVERIFY(QDir(root.path()).mkdir(name));
    folders.append(uriOf(root.filePath(name)));
  }
  const RevealPlan atBound =
      planReveal(RevealKind::Folders, folders.mid(0, maximumRevealWindows));
  QVERIFY2(atBound.ok(), qPrintable(atBound.diagnostic));
  QCOMPARE(atBound.requests.size(), maximumRevealWindows);
  const RevealPlan tooManyWindows = planReveal(RevealKind::Folders, folders);
  QCOMPARE(tooManyWindows.error, RevealError::TooMany);
  QVERIFY(tooManyWindows.requests.isEmpty());

  const QStringList repeated(maximumRevealUris + 1, folders.constFirst());
  QCOMPARE(planReveal(RevealKind::Folders, repeated).error, RevealError::TooMany);
  QVERIFY(planReveal(RevealKind::Folders, repeated.mid(1)).ok());
}

void TestRevealRequest::namesMustBeSingleEntries() {
  for (const QString &name : {QStringLiteral("a"), QStringLiteral("a b"), QStringLiteral("-x"),
                              QStringLiteral(".hidden"), QStringLiteral("back\\slash"),
                              QStringLiteral("…")}) {
    QVERIFY2(isRevealableName(name), qPrintable(name));
  }
  for (const QString &name :
       {QString(), QStringLiteral("."), QStringLiteral(".."), QStringLiteral("a/b"),
        QStringLiteral("/"), QStringLiteral("a") + QChar::Null + QStringLiteral("b")}) {
    QVERIFY2(!isRevealableName(name), qPrintable(name));
  }

  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("A")));
  // "." and ".." name no entry of a folder.
  const QString base = QStringLiteral("file://") +
                       QUrl::fromLocalFile(root.filePath(QStringLiteral("A"))).path(
                           QUrl::FullyEncoded);
  QCOMPARE(planReveal(RevealKind::Items, {base + QStringLiteral("/..")}).error,
           RevealError::NotFound);
  QCOMPARE(planReveal(RevealKind::Items, {base + QStringLiteral("/.")}).error,
           RevealError::NotFound);
}

void TestRevealRequest::onlyTheDocumentedActionsMayFollowAReveal() {
  for (const char *action : {"file.properties", "file.open-with", "file.new-file"}) {
    QVERIFY2(isRevealAction(QLatin1String(action)), action);
  }
  // A destructive or unknown action never runs from a command line.
  for (const char *action : {"", "file.trash", "file.empty-trash", "file.delete", "edit.paste",
                             "application.keep-in-dock", "file.properties ", "FILE.PROPERTIES"}) {
    QVERIFY2(!isRevealAction(QLatin1String(action)), action);
  }
}

QTEST_GUILESS_MAIN(TestRevealRequest)
#include "tst_reveal_request.moc"
