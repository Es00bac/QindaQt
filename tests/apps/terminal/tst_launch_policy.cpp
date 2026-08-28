// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_launch_policy.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using QindaQt::Apps::Terminal::TerminalLaunchPolicy;

namespace {

QStringList baseEnvironment() {
  return {QStringLiteral("PATH=/usr/bin"),
          QStringLiteral("HOME=/home/tester"),
          QStringLiteral("LANG=en_US.UTF-8")};
}

} // namespace

class TerminalLaunchPolicyTest final : public QObject {
  Q_OBJECT

private slots:
  void resolvesExplicitProgramWithArgvSemantics();
  void fallsBackToShellVariableThenDefault();
  void rejectsRelativeAndHostilePrograms();
  void rejectsHostileArguments();
  void validatesWorkingDirectory();
  void forcesTermAndColorTermOverHostileInheritedValues();
  void appendsUtf8LocaleFallbackOnlyWhenMissing();
  void dropsMalformedEnvironmentEntries();
  void rejectsOversizedEnvironments();
  void clampsHostileViewSizes();
  void executableFileCheckUsesRealMetadata();
};

void TerminalLaunchPolicyTest::resolvesExplicitProgramWithArgvSemantics() {
  const auto resolution = TerminalLaunchPolicy::resolveShell(
      QCoreApplication::applicationFilePath(),
      {QStringLiteral("-l"), QStringLiteral("two words")},
      QString(), baseEnvironment());
  QVERIFY(resolution.outcome.ok);
  QCOMPARE(resolution.request.program,
           QCoreApplication::applicationFilePath());
  // Arguments survive verbatim and unjoined; "two words" is one argv entry.
  QCOMPARE(resolution.request.arguments.size(), 2);
  QCOMPARE(resolution.request.arguments.at(1), QStringLiteral("two words"));
  QVERIFY(resolution.request.environment.isEmpty());
}

void TerminalLaunchPolicyTest::fallsBackToShellVariableThenDefault() {
  auto fromVariable = TerminalLaunchPolicy::resolveShell(
      QString(), {}, QString(),
      {QStringLiteral("SHELL=") + QCoreApplication::applicationFilePath()});
  QVERIFY(fromVariable.outcome.ok);
  QCOMPARE(fromVariable.request.program,
           QCoreApplication::applicationFilePath());

  // The default shell only needs to pass validation when it exists; a
  // minimal container may not ship it, so the assertion is conditional on
  // real filesystem metadata.
  const QFileInfo fallback(TerminalLaunchPolicy::kDefaultShell);
  if (fallback.isExecutable()) {
    auto fromDefault = TerminalLaunchPolicy::resolveShell(
        QString(), {}, QString(), {});
    QVERIFY(fromDefault.outcome.ok);
    QCOMPARE(fromDefault.request.program,
             TerminalLaunchPolicy::kDefaultShell);
  }
}

void TerminalLaunchPolicyTest::rejectsRelativeAndHostilePrograms() {
  QStringList hostile{QStringLiteral("sh"),              // relative
                      QStringLiteral("/bin/sh\nrm"),     // embedded newline
                      QStringLiteral("/bin/sh -c evil"), // injected shape
                      QString()};                        // empty
  for (const QString &program : hostile) {
    const auto resolution =
        TerminalLaunchPolicy::resolveShell(program, {}, QString(), {});
    QVERIFY2(!resolution.outcome.ok, qPrintable(program));
    QVERIFY(!resolution.outcome.diagnostic.isEmpty());
    QVERIFY(resolution.request.program.isEmpty());
  }

  const auto directory = TerminalLaunchPolicy::resolveShell(
      QStringLiteral("/tmp"), {}, QString(), {});
  QVERIFY(!directory.outcome.ok);
  QVERIFY(directory.outcome.diagnostic.contains(
      QLatin1String("not executable")));

  const auto missing = TerminalLaunchPolicy::resolveShell(
      QStringLiteral("/definitely/not/present/qindaqt-shell"), {}, QString(),
      {});
  QVERIFY(!missing.outcome.ok);
  QVERIFY(missing.outcome.diagnostic.contains(QLatin1String("exist")));
}

void TerminalLaunchPolicyTest::rejectsHostileArguments() {
  const QString program = QCoreApplication::applicationFilePath();

  const auto controlCharacter = TerminalLaunchPolicy::resolveShell(
      program, {QStringLiteral("safe"), QStringLiteral("bad\nargument")},
      QString(), {});
  QVERIFY(!controlCharacter.outcome.ok);
  QVERIFY(controlCharacter.outcome.diagnostic.contains(
      QLatin1String("control character")));

  const QString longArgument(kMaxArgumentLength + 1, QLatin1Char('a'));
  const auto oversized =
      TerminalLaunchPolicy::resolveShell(program, {longArgument}, QString(),
                                         {});
  QVERIFY(!oversized.outcome.ok);
  QVERIFY(oversized.outcome.diagnostic.contains(
      QLatin1String("exceeds")));

  QStringList tooMany;
  for (int index = 0; index <= TerminalLaunchPolicy::kMaxArguments; ++index) {
    tooMany.append(QStringLiteral("a%1").arg(index));
  }
  const auto count = TerminalLaunchPolicy::resolveShell(program, tooMany,
                                                        QString(), {});
  QVERIFY(!count.outcome.ok);
  QVERIFY(count.outcome.diagnostic.contains(QLatin1String("Too many")));
}

void TerminalLaunchPolicyTest::validatesWorkingDirectory() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  const auto ok = TerminalLaunchPolicy::resolveShell(
      QCoreApplication::applicationFilePath(), {}, directory.path(),
      baseEnvironment());
  QVERIFY(ok.outcome.ok);
  QCOMPARE(ok.request.workingDirectory, directory.path());

  const auto missing = TerminalLaunchPolicy::resolveShell(
      QCoreApplication::applicationFilePath(), {},
      QStringLiteral("/definitely/not/a/directory"), baseEnvironment());
  QVERIFY(!missing.outcome.ok);
  QVERIFY(missing.outcome.diagnostic.contains(
      QLatin1String("directory")));

  const auto inherited = TerminalLaunchPolicy::resolveShell(
      QCoreApplication::applicationFilePath(), {}, QString(),
      baseEnvironment());
  QVERIFY(inherited.outcome.ok);
  QVERIFY(inherited.request.workingDirectory.isEmpty());
}

void TerminalLaunchPolicyTest::
    forcesTermAndColorTermOverHostileInheritedValues() {
  auto environment = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("TERM="), // hostile empty
       QStringLiteral("TERM=dumb"),
       QStringLiteral("COLORTERM=\nmalicious\npayload"),
       QStringLiteral("LANG=en_US.UTF-8"),
       QStringLiteral("PATH=/usr/bin")});
  QVERIFY(environment.outcome.ok);

  bool sawTerm = false;
  bool sawColorTerm = false;
  for (const QString &entry : environment.environment) {
    if (entry.startsWith(QLatin1String("TERM="))) {
      sawTerm = true;
      QCOMPARE(entry, TerminalLaunchPolicy::kTermVariable +
                          QLatin1Char('=') +
                          TerminalLaunchPolicy::kTermValue);
    }
    if (entry.startsWith(QLatin1String("COLORTERM="))) {
      sawColorTerm = true;
      QCOMPARE(entry, TerminalLaunchPolicy::kColorTermVariable +
                          QLatin1Char('=') +
                          TerminalLaunchPolicy::kColorTermValue);
    }
  }
  QVERIFY(sawTerm);
  QVERIFY(sawColorTerm);
  // Exactly one of each: hostile inherited values are removed, never kept.
  QCOMPARE(environment.environment.filter(
               QRegularExpression(QStringLiteral("^TERM=")))
               .size(),
           1);
}

void TerminalLaunchPolicyTest::
    appendsUtf8LocaleFallbackOnlyWhenMissing() {
  auto withUtf8 = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LANG=en_US.UTF-8")});
  QVERIFY(withUtf8.outcome.ok);
  QVERIFY(!withUtf8.environment.filter(
               QRegularExpression(QStringLiteral("^LANG=")))
               .isEmpty());
  QCOMPARE(withUtf8.environment.filter(
               QRegularExpression(QStringLiteral("^LANG=")))
               .size(),
           1);

  auto withoutUtf8 = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LANG=C"), QStringLiteral("PATH=/usr/bin")});
  QVERIFY(withoutUtf8.outcome.ok);
  const auto appended = withoutUtf8.environment.filter(
      QRegularExpression(QStringLiteral("^LANG=")));
  QCOMPARE(appended.size(), 1);
  QCOMPARE(appended.first(),
           QStringLiteral("LANG=") +
               TerminalLaunchPolicy::kUtf8FallbackLocale);

  auto empty = TerminalLaunchPolicy::childEnvironment({});
  QVERIFY(empty.outcome.ok);
  QVERIFY(!empty.environment.filter(
               QRegularExpression(QStringLiteral("^LANG=")))
               .isEmpty());
  QVERIFY(!empty.environment.filter(
               QRegularExpression(QStringLiteral("^TERM=")))
               .isEmpty());

  auto latin1Only = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LC_ALL=de_DE.ISO-8859-1")});
  QVERIFY(latin1Only.outcome.ok);
  QCOMPARE(latin1Only.environment.filter(
               QRegularExpression(QStringLiteral("^LANG=")))
               .first(),
           QStringLiteral("LANG=") +
               TerminalLaunchPolicy::kUtf8FallbackLocale);
}

void TerminalLaunchPolicyTest::dropsMalformedEnvironmentEntries() {
  auto environment = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("no-equals-sign"),
       QStringLiteral("=empty-key"),
       QStringLiteral("BAD-KEY=value"), // invalid key characters
       QStringLiteral("GOOD-KEY=1"),
       QStringLiteral("MULTILINE=first\nsecond"),
       QStringLiteral("9NUMBERED=value"),
       QStringLiteral("PATH=/usr/bin")});
  QVERIFY(environment.outcome.ok);
  for (const QString &entry : environment.environment) {
    QVERIFY2(!entry.contains(QLatin1Char('\n')), qPrintable(entry));
    QVERIFY2(entry != QStringLiteral("BAD-KEY=value"), qPrintable(entry));
    QVERIFY2(!entry.startsWith(QLatin1String("9NUMBERED=")),
             qPrintable(entry));
  }
  QVERIFY(environment.environment.contains(QStringLiteral("GOOD-KEY=1")));
  QVERIFY(environment.environment.contains(QStringLiteral("PATH=/usr/bin")));
}

void TerminalLaunchPolicyTest::rejectsOversizedEnvironments() {
  QStringList huge;
  for (int index = 0; index <= TerminalLaunchPolicy::kMaxEnvironmentEntries;
       ++index) {
    huge.append(QStringLiteral("KEY%1=v").arg(index));
  }
  auto rejected = TerminalLaunchPolicy::childEnvironment(huge);
  QVERIFY(!rejected.outcome.ok);
  QVERIFY(rejected.environment.isEmpty());

  const QString longValue(TerminalLaunchPolicy::kMaxEnvironmentEntryLength + 1,
                          QLatin1Char('x'));
  auto longEntry = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LONG=") + longValue, QStringLiteral("PATH=/usr/bin")});
  QVERIFY(longEntry.outcome.ok);
  QVERIFY(longEntry.environment.filter(
              QRegularExpression(QStringLiteral("^LONG=")))
              .isEmpty());
}

void TerminalLaunchPolicyTest::clampsHostileViewSizes() {
  const auto zero = TerminalLaunchPolicy::clampViewSize(0, 0);
  QCOMPARE(zero, QSize(TerminalLaunchPolicy::kMinViewWidth,
                       TerminalLaunchPolicy::kMinViewHeight));

  const auto negative = TerminalLaunchPolicy::clampViewSize(-500, -500);
  QCOMPARE(negative.width(), TerminalLaunchPolicy::kMinViewWidth);
  QCOMPARE(negative.height(), TerminalLaunchPolicy::kMinViewHeight);

  const auto huge = TerminalLaunchPolicy::clampViewSize(1 << 24, 1 << 24);
  QCOMPARE(huge.width(), TerminalLaunchPolicy::kMaxViewWidth);
  QCOMPARE(huge.height(), TerminalLaunchPolicy::kMaxViewHeight);

  const auto mixed = TerminalLaunchPolicy::clampViewSize(-1, 800);
  QCOMPARE(mixed.width(), TerminalLaunchPolicy::kMinViewWidth);
  QCOMPARE(mixed.height(), 800);
}

void TerminalLaunchPolicyTest::executableFileCheckUsesRealMetadata() {
  // A non-executable regular file must be rejected by the metadata check.
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("plain.txt"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();
  QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);

  const auto resolution =
      TerminalLaunchPolicy::resolveShell(path, {}, QString(), {});
  QVERIFY(!resolution.outcome.ok);
  QVERIFY(resolution.outcome.diagnostic.contains(
      QLatin1String("not executable")));
}

QTEST_MAIN(TerminalLaunchPolicyTest)
#include "tst_launch_policy.moc"
