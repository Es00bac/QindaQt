// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/local_mutation_backend.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

class TestNoReplaceRace final : public QObject {
  Q_OBJECT

private slots:
  void racingWriterCannotBeOverwritten();
};

void TestNoReplaceRace::racingWriterCannotBeOverwritten() {
  // AGENT-NOTE: Regression for review P2-2. The preloaded writer creates the
  // destination after the backend's absence check; commit must atomically
  // return already-exists and preserve both writers' bytes.
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString source = fixture.filePath(QStringLiteral("source"));
  const QString destination = fixture.filePath(QStringLiteral("dest"));
  QFile input(source);
  QVERIFY(input.open(QIODevice::WriteOnly));
  QCOMPARE(input.write("user-data"), 9);
  input.close();

  MutationRequest request;
  request.kind = MutationKind::Rename;
  request.sourcePath = source;
  request.destinationPath = destination;
  request.declaredRoots = {fixture.path()};
  request.expectedSource = LocalMutationBackend::identityForPath(source);
  request.expectedParent = LocalMutationBackend::identityForPath(fixture.path());
  LocalMutationBackend backend(fixture.filePath(QStringLiteral("Trash")));
  const MutationResult result = backend.execute(
      request, std::make_shared<std::atomic_bool>(false), {});

  QCOMPARE(result.error, MutationError::AlreadyExists);
  QVERIFY(QFileInfo::exists(source));
  QFile attacker(destination);
  QVERIFY(attacker.open(QIODevice::ReadOnly));
  QCOMPARE(attacker.readAll(), QByteArray("attacker-content"));
}

QTEST_APPLESS_MAIN(TestNoReplaceRace)
#include "tst_no_replace_race.moc"
