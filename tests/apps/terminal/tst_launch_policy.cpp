// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_launch_policy.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

using QindaQt::Apps::Terminal::TerminalLaunchPolicy;

namespace {

QStringList baseEnvironment() {
  return {QStringLiteral("PATH=/usr/bin"),
          QStringLiteral("HOME=/home/tester"),
          QStringLiteral("LANG=en_US.UTF-8")};
}

// Mirrors child getenv over an execve envp array: the first matching entry
// decides, which is exactly the authority the policy must control.
std::optional<QString> firstEnvValue(const QStringList &environment,
                                     const QString &name) {
  const QString prefix = name + QLatin1Char('=');
  for (const QString &entry : environment) {
    if (entry.startsWith(prefix)) {
      return entry.mid(prefix.size());
    }
  }
  return std::nullopt;
}

int envEntryCount(const QStringList &environment, const QString &name) {
  const QString prefix = name + QLatin1Char('=');
  int count = 0;
  for (const QString &entry : environment) {
    if (entry.startsWith(prefix)) {
      ++count;
    }
  }
  return count;
}

bool selectsUtf8(const QString &value) {
  const QString upper = value.toUpper();
  return upper.contains(QLatin1String("UTF-8")) ||
         upper.contains(QLatin1String("UTF8"));
}

// Applies libc locale precedence (LC_ALL > LC_CTYPE > LANG) to the produced
// environment and reports whether the effective character set is UTF-8. The
// tests assert this effective outcome, never a mere appended string.
bool effectiveLocaleIsUtf8(const QStringList &environment) {
  static const QString kVariables[3] = {QStringLiteral("LC_ALL"),
                                        QStringLiteral("LC_CTYPE"),
                                        QStringLiteral("LANG")};
  for (const QString &name : kVariables) {
    const auto value = firstEnvValue(environment, name);
    if (value.has_value()) {
      return selectsUtf8(*value);
    }
  }
  // No locale selection at all is not a UTF-8 guarantee.
  return false;
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
  void effectiveLocaleAuthorityIsUtf8UnderHostileInheritance();
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
  // An empty program is not hostile here: it legitimately falls back to
  // $SHELL/default, which fallsBackToShellVariableThenDefault covers.
  QStringList hostile{QStringLiteral("sh"),          // relative
                      QStringLiteral("/bin/sh\nrm"), // embedded newline
                      QStringLiteral("/bin/sh -c evil")};// injected shape
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
  QVERIFY(directory.outcome.diagnostic.contains(QLatin1String("directory")));

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

  const QString longArgument(
      TerminalLaunchPolicy::kMaxArgumentLength + 1, QLatin1Char('a'));
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

  // Effective authority: exactly one entry per forced variable and it must
  // be the first (child getenv semantics), never a surviving inherited one.
  QCOMPARE(envEntryCount(environment.environment,
                         TerminalLaunchPolicy::kTermVariable),
           1);
  QCOMPARE(envEntryCount(environment.environment,
                         TerminalLaunchPolicy::kColorTermVariable),
           1);
  QCOMPARE(firstEnvValue(environment.environment,
                         TerminalLaunchPolicy::kTermVariable),
           std::optional<QString>(TerminalLaunchPolicy::kTermValue));
  QCOMPARE(firstEnvValue(environment.environment,
                         TerminalLaunchPolicy::kColorTermVariable),
           std::optional<QString>(TerminalLaunchPolicy::kColorTermValue));
  QVERIFY(!environment.environment.join(QLatin1Char('\n')).contains(
      QLatin1String("dumb")));
}

void TerminalLaunchPolicyTest::
    effectiveLocaleAuthorityIsUtf8UnderHostileInheritance() {
  // A non-UTF-8 LC_ALL governs even when LANG selects UTF-8; the policy must
  // replace the effective authority variable itself.
  auto hostileAll = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LC_ALL=de_DE.ISO-8859-1"),
       QStringLiteral("LANG=en_US.UTF-8"), QStringLiteral("PATH=/usr/bin")});
  QVERIFY(hostileAll.outcome.ok);
  QVERIFY(effectiveLocaleIsUtf8(hostileAll.environment));
  QCOMPARE(firstEnvValue(hostileAll.environment,
                         QStringLiteral("LC_ALL")),
           std::optional<QString>(
               TerminalLaunchPolicy::kUtf8FallbackLocale));
  QCOMPARE(envEntryCount(hostileAll.environment,
                         QStringLiteral("LC_ALL")),
           1);
  QVERIFY(!hostileAll.environment.join(QLatin1Char('\n')).contains(
      QLatin1String("ISO-8859-1")));

  // A UTF-8 LC_ALL governs over a hostile LC_CTYPE; the lower variables are
  // preserved untouched because they cannot override it.
  auto governingAll = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LC_ALL=en_US.UTF-8"), QStringLiteral("LC_CTYPE=C"),
       QStringLiteral("LANG=C")});
  QVERIFY(governingAll.outcome.ok);
  QVERIFY(effectiveLocaleIsUtf8(governingAll.environment));
  QCOMPARE(firstEnvValue(governingAll.environment,
                         QStringLiteral("LC_ALL")),
           std::optional<QString>(QStringLiteral("en_US.UTF-8")));
  QCOMPARE(firstEnvValue(governingAll.environment,
                         QStringLiteral("LC_CTYPE")),
           std::optional<QString>(QStringLiteral("C")));

  // Without LC_ALL, a non-UTF-8 LC_CTYPE decides and must be replaced even
  // when LANG selects UTF-8.
  auto hostileCtype = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LC_CTYPE=C"), QStringLiteral("LANG=en_US.UTF-8")});
  QVERIFY(hostileCtype.outcome.ok);
  QVERIFY(effectiveLocaleIsUtf8(hostileCtype.environment));
  QCOMPARE(firstEnvValue(hostileCtype.environment,
                         QStringLiteral("LC_CTYPE")),
           std::optional<QString>(
               TerminalLaunchPolicy::kUtf8FallbackLocale));
  QCOMPARE(firstEnvValue(hostileCtype.environment,
                         QStringLiteral("LANG")),
           std::optional<QString>(QStringLiteral("en_US.UTF-8")));

  // Without LC_ALL/LC_CTYPE, a non-UTF-8 LANG is the effective authority.
  auto hostileLang = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LANG=C"), QStringLiteral("PATH=/usr/bin")});
  QVERIFY(hostileLang.outcome.ok);
  QVERIFY(effectiveLocaleIsUtf8(hostileLang.environment));
  QCOMPARE(firstEnvValue(hostileLang.environment, QStringLiteral("LANG")),
           std::optional<QString>(
               TerminalLaunchPolicy::kUtf8FallbackLocale));
  QCOMPARE(envEntryCount(hostileLang.environment, QStringLiteral("LANG")),
           1);

  // Minimal sessions with no locale selection receive the fallback.
  auto minimal = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("PATH=/usr/bin")});
  QVERIFY(minimal.outcome.ok);
  QVERIFY(effectiveLocaleIsUtf8(minimal.environment));
  QCOMPARE(firstEnvValue(minimal.environment, QStringLiteral("LANG")),
           std::optional<QString>(
               TerminalLaunchPolicy::kUtf8FallbackLocale));

  // An already-UTF-8 authority is preserved, never rewritten.
  auto alreadyUtf8 = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LC_ALL=C.UTF-8")});
  QVERIFY(alreadyUtf8.outcome.ok);
  QCOMPARE(firstEnvValue(alreadyUtf8.environment,
                         QStringLiteral("LC_ALL")),
           std::optional<QString>(QStringLiteral("C.UTF-8")));

  // Duplicate hostile authorities collapse to one forced UTF-8 entry.
  auto duplicates = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("LC_ALL=C"), QStringLiteral("LC_ALL=en_US.UTF-8")});
  QVERIFY(duplicates.outcome.ok);
  QVERIFY(effectiveLocaleIsUtf8(duplicates.environment));
  QCOMPARE(envEntryCount(duplicates.environment,
                         QStringLiteral("LC_ALL")),
           1);
}

void TerminalLaunchPolicyTest::dropsMalformedEnvironmentEntries() {
  auto environment = TerminalLaunchPolicy::childEnvironment(
      {QStringLiteral("no-equals-sign"),
       QStringLiteral("=empty-key"),
       QStringLiteral("BAD-KEY=value"), // invalid key characters
       QStringLiteral("GOOD_KEY=1"),
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
  QVERIFY(environment.environment.contains(QStringLiteral("GOOD_KEY=1")));
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
