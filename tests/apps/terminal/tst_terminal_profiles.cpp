// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiles/terminal_profile.h"
#include "session/terminal_session_collection.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtTest>

using namespace QindaQt::Apps::Terminal;

namespace {

TerminalProfile userProfile(const QString &id = QStringLiteral("work")) {
  TerminalProfile profile = builtinDefaultProfile();
  profile.id = id;
  profile.name = QStringLiteral("Work");
  profile.shellProgram = QStringLiteral("/bin/echo");
  profile.shellArguments = {QStringLiteral("literal;$(never-executed)")};
  profile.fontFamily = QStringLiteral("Monospace");
  profile.fontSize = 12;
  profile.colorSchemeId = QStringLiteral("qinda-light");
  profile.scrollbackLines = 4096;
  profile.bellPolicy = TerminalProfile::BellPolicy::Audible;
  return profile;
}

QJsonObject encodedObject(const TerminalProfile &profile) {
  bool ok = false;
  const auto document =
      QJsonDocument::fromJson(encodeTerminalProfiles({profile}, &ok).toUtf8());
  Q_ASSERT(ok);
  return document.array().first().toObject();
}

} // namespace

class TerminalProfilesTest final : public QObject {
  Q_OBJECT

private slots:
  void builtInAndUserProfileValidate();
  void hostileValuesAreRejected();
  void listBoundsAndIdentityAreAtomic();
  void codecRoundTripsAndRejectsHostileDocuments();
  void generatedIdentifiersMatchThePublishedContract();
  void launchResolutionKeepsArgumentsLiteralAndEnvironmentOwned();
};

void TerminalProfilesTest::builtInAndUserProfileValidate() {
  QVERIFY(validateTerminalProfile(builtinDefaultProfile()).ok);
  QVERIFY(validateTerminalProfile(userProfile()).ok);
  QCOMPARE(builtinDefaultProfile().scrollbackLines,
           TerminalProfile::kDefaultScrollbackLines);
  QCOMPARE(builtinDefaultProfile().bellPolicy,
           TerminalProfile::BellPolicy::Silent);
}

void TerminalProfilesTest::hostileValuesAreRejected() {
  auto profile = userProfile();
  profile.id = QStringLiteral("../escape");
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.name = QStringLiteral(" padded ");
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.name = QString(QChar(0xD800));
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.shellProgram = QStringLiteral("relative-shell");
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.shellArguments = {QStringLiteral("line\nbreak")};
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.fontSize = TerminalProfile::kMaxFontSize + 1;
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.colorSchemeId = QStringLiteral("../../theme");
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.scrollbackLines = TerminalProfile::kMaxScrollbackLines + 1;
  QVERIFY(!validateTerminalProfile(profile).ok);

  profile = userProfile();
  profile.bellPolicy = static_cast<TerminalProfile::BellPolicy>(99);
  QVERIFY(!validateTerminalProfile(profile).ok);
}

void TerminalProfilesTest::listBoundsAndIdentityAreAtomic() {
  QList<TerminalProfile> profiles;
  for (int index = 0; index < TerminalProfile::kMaxUserProfiles; ++index) {
    profiles.append(userProfile(QStringLiteral("profile-%1").arg(index)));
  }
  QVERIFY(validateTerminalProfileList(profiles).ok);

  profiles.append(userProfile(QStringLiteral("one-too-many")));
  QVERIFY(!validateTerminalProfileList(profiles).ok);

  profiles = {userProfile(QStringLiteral("same")),
              userProfile(QStringLiteral("same"))};
  QVERIFY(!validateTerminalProfileList(profiles).ok);

  profiles = {userProfile(builtinDefaultProfileId())};
  QVERIFY(!validateTerminalProfileList(profiles).ok);
}

void TerminalProfilesTest::codecRoundTripsAndRejectsHostileDocuments() {
  const QList<TerminalProfile> profiles{userProfile()};
  bool ok = false;
  const QString encoded = encodeTerminalProfiles(profiles, &ok);
  QVERIFY(ok);
  const auto decoded = decodeTerminalProfiles(encoded);
  QVERIFY(decoded.ok);
  QCOMPARE(decoded.profiles, profiles);

  QJsonObject fractional = encodedObject(userProfile());
  fractional.insert(QStringLiteral("scrollbackLines"), 1.5);
  QVERIFY(!decodeTerminalProfiles(
               QString::fromUtf8(QJsonDocument(QJsonArray{fractional})
                                     .toJson(QJsonDocument::Compact)))
               .ok);

  QJsonObject wrongArgumentType = encodedObject(userProfile());
  wrongArgumentType.insert(QStringLiteral("shellArguments"),
                           QJsonArray{QStringLiteral("safe"), 7});
  QVERIFY(!decodeTerminalProfiles(
               QString::fromUtf8(QJsonDocument(QJsonArray{wrongArgumentType})
                                     .toJson(QJsonDocument::Compact)))
               .ok);

  const QJsonObject duplicate = encodedObject(userProfile());
  const auto duplicateResult = decodeTerminalProfiles(
      QString::fromUtf8(QJsonDocument(QJsonArray{duplicate, duplicate})
                            .toJson(QJsonDocument::Compact)));
  QVERIFY(!duplicateResult.ok);
  QVERIFY(duplicateResult.profiles.isEmpty());

  QVERIFY(!decodeTerminalProfiles(QStringLiteral("{}")).ok);
}

void TerminalProfilesTest::generatedIdentifiersMatchThePublishedContract() {
  const QString first = generateProfileId();
  const QString second = generateProfileId();
  QCOMPARE(first.size(), 32);
  QVERIFY(QRegularExpression(QStringLiteral("^[a-f0-9]{32}$"))
              .match(first)
              .hasMatch());
  QVERIFY(first != second);
  QVERIFY(validateTerminalProfile(userProfile(first)).ok);
}

void TerminalProfilesTest::
    launchResolutionKeepsArgumentsLiteralAndEnvironmentOwned() {
  const auto profile = userProfile();
  const auto resolved = resolveProfileLaunch(
      profile, QStringLiteral("/bin/true"), {}, {},
      {QStringLiteral("TERM=host"), QStringLiteral("COLORTERM=host"),
       QStringLiteral("SAFE=value")});
  QVERIFY(resolved.outcome.ok);
  QCOMPARE(resolved.request.program, QStringLiteral("/bin/echo"));
  QCOMPARE(resolved.request.arguments, profile.shellArguments);
  QVERIFY(resolved.request.environment.contains(QStringLiteral("SAFE=value")));
  QVERIFY(resolved.request.environment.contains(
      QStringLiteral("TERM=xterm-256color")));
  QVERIFY(resolved.request.environment.contains(
      QStringLiteral("COLORTERM=truecolor")));
  QVERIFY(!resolved.request.environment.contains(QStringLiteral("TERM=host")));

  auto inherited = builtinDefaultProfile();
  const auto fallback = resolveProfileLaunch(
      inherited, QStringLiteral("/bin/true"), {QStringLiteral("--")}, {}, {});
  QVERIFY(fallback.outcome.ok);
  QCOMPARE(fallback.request.program, QStringLiteral("/bin/true"));
  QCOMPARE(fallback.request.arguments, QStringList{QStringLiteral("--")});
}

QTEST_MAIN(TerminalProfilesTest)
#include "tst_terminal_profiles.moc"
