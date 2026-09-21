// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_login_screen/sddm_owned_config.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::SettingsLoginScreen;

class SddmOwnedConfigTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void serializeDeserializeRoundTrip();
  void deserializeRejectsForeignKey();
  void deserializeRejectsForeignSection();
  void deserializeRejectsMalformedAndEmpty();
  void deserializeRejectsUnsafeValue();
  void mergeReplacesInPlaceAndPreservesTheRest();
  void mergeAppendsIntoExistingSectionAndCreatesMissingSections();
  void mergePatchesTheLastDuplicate();
  void mergeKeepsTrailingNewlineState();
  void validateRejectsBadValuesAndNamesThem();
  void validateAcceptsEmptyAutologinValues();
  void writeCreatesMergesAndKeepsPermissions();
};

void SddmOwnedConfigTest::serializeDeserializeRoundTrip() {
  SddmOwnedChangeSet changes;
  changes.theme = QStringLiteral("qinda-reclaimed");
  changes.numlock = QStringLiteral("on");
  changes.cursorTheme = QStringLiteral("breeze_cursors");
  changes.autologinUser = QStringLiteral("ada");
  changes.autologinSession = QStringLiteral("plasma.desktop");

  const QString text = serializeOwnedChangeSet(changes);
  QString error;
  const auto parsed = deserializeOwnedChangeSet(text, &error);
  QVERIFY2(parsed.has_value(), qPrintable(error));
  QCOMPARE(*parsed->theme, QStringLiteral("qinda-reclaimed"));
  QCOMPARE(*parsed->numlock, QStringLiteral("on"));
  QCOMPARE(*parsed->cursorTheme, QStringLiteral("breeze_cursors"));
  QCOMPARE(*parsed->autologinUser, QStringLiteral("ada"));
  QCOMPARE(*parsed->autologinSession, QStringLiteral("plasma.desktop"));
}

void SddmOwnedConfigTest::deserializeRejectsForeignKey() {
  QString error;
  const auto parsed = deserializeOwnedChangeSet(
      QStringLiteral("[Theme]\nFont=Hack\n"), &error);
  QVERIFY(!parsed.has_value());
  QVERIFY(error.contains(QStringLiteral("Font")));
}

void SddmOwnedConfigTest::deserializeRejectsForeignSection() {
  QString error;
  const auto parsed = deserializeOwnedChangeSet(
      QStringLiteral("[X11]\nServerPath=/usr/bin/X\n"), &error);
  QVERIFY(!parsed.has_value());
  QVERIFY(error.contains(QStringLiteral("X11")));
}

void SddmOwnedConfigTest::deserializeRejectsMalformedAndEmpty() {
  QString error;
  QVERIFY(!deserializeOwnedChangeSet(QStringLiteral("not-ini\n"), &error)
               .has_value());
  QVERIFY(!deserializeOwnedChangeSet(QStringLiteral("\n# nothing\n"), &error)
               .has_value());
}

void SddmOwnedConfigTest::deserializeRejectsUnsafeValue() {
  QString error;
  // The guard in isSafeValue() names NUL and line breaks; embed a real NUL.
  // (A QStringLiteral escape would need octal -- "\0" -- and reads as noise.)
  const QString payload = QStringLiteral("[Theme]\nCurrent=qinda") +
                          QChar(QChar::Null) + QStringLiteral("reclaimed\n");
  const auto parsed = deserializeOwnedChangeSet(payload, &error);
  QVERIFY(!parsed.has_value());
}

void SddmOwnedConfigTest::mergeReplacesInPlaceAndPreservesTheRest() {
  const QString existing = QStringLiteral(
      "# Managed by QindaQt Settings.\n"
      "[Theme]\n"
      "Current=old-theme\n"
      "CursorTheme=breeze_cursors\n"
      "\n"
      "[Users]\n"
      "MinimumUid=1000\n");
  SddmOwnedChangeSet changes;
  changes.theme = QStringLiteral("qinda-prism");

  const QString merged = mergeOwnedChangeSetIntoConfigText(existing, changes);
  QVERIFY(merged.contains(QStringLiteral("Current=qinda-prism")));
  QVERIFY(!merged.contains(QStringLiteral("Current=old-theme")));
  // The comment, the other owned-but-unchanged key, and the foreign section
  // all survive verbatim.
  QVERIFY(merged.contains(QStringLiteral("# Managed by QindaQt Settings.")));
  QVERIFY(merged.contains(QStringLiteral("CursorTheme=breeze_cursors")));
  QVERIFY(merged.contains(QStringLiteral("[Users]\nMinimumUid=1000")));
}

void SddmOwnedConfigTest::
    mergeAppendsIntoExistingSectionAndCreatesMissingSections() {
  const QString existing = QStringLiteral(
      "[Theme]\n"
      "Current=qinda-washi\n"
      "\n"
      "[Users]\n"
      "MinimumUid=1000\n");
  SddmOwnedChangeSet changes;
  changes.cursorTheme = QStringLiteral("whiteglass");
  changes.autologinUser = QStringLiteral("ada");

  const QString merged = mergeOwnedChangeSetIntoConfigText(existing, changes);
  // CursorTheme lands inside the existing [Theme] block, before [Users].
  const qsizetype themeSection = merged.indexOf(QStringLiteral("[Theme]"));
  const qsizetype cursorLine = merged.indexOf(QStringLiteral("CursorTheme=whiteglass"));
  const qsizetype usersSection = merged.indexOf(QStringLiteral("[Users]"));
  QVERIFY(themeSection >= 0 && cursorLine > themeSection &&
          cursorLine < usersSection);
  // A brand-new section is appended after everything existing.
  const qsizetype autologinSection = merged.indexOf(QStringLiteral("[Autologin]"));
  const qsizetype userLine = merged.indexOf(QStringLiteral("User=ada"));
  QVERIFY(autologinSection > usersSection);
  QVERIFY(userLine > autologinSection);
}

void SddmOwnedConfigTest::mergePatchesTheLastDuplicate() {
  // A hand-duplicated key: SDDM honors the last one, so that is the one the
  // merge must patch for the write to take effect.
  const QString existing = QStringLiteral(
      "[Theme]\n"
      "Current=first\n"
      "\n"
      "[Theme]\n"
      "Current=second\n");
  SddmOwnedChangeSet changes;
  changes.theme = QStringLiteral("qinda-night-patrol");

  const QString merged = mergeOwnedChangeSetIntoConfigText(existing, changes);
  const qsizetype firstOld = merged.indexOf(QStringLiteral("Current=first"));
  const qsizetype patched =
      merged.indexOf(QStringLiteral("Current=qinda-night-patrol"));
  const qsizetype secondOld = merged.indexOf(QStringLiteral("Current=second"));
  QVERIFY(firstOld >= 0);          // untouched earlier duplicate preserved
  QVERIFY(patched > firstOld);     // the later line is the patched one
  QCOMPARE(secondOld, qsizetype(-1));
}

void SddmOwnedConfigTest::mergeKeepsTrailingNewlineState() {
  SddmOwnedChangeSet changes;
  changes.numlock = QStringLiteral("off");
  const QString withNewline =
      mergeOwnedChangeSetIntoConfigText(QStringLiteral("[General]\n"), changes);
  QVERIFY(withNewline.endsWith(u'\n'));
  QVERIFY(!withNewline.endsWith(QStringLiteral("\n\n")));
}

void SddmOwnedConfigTest::validateRejectsBadValuesAndNamesThem() {
  const QStringList themes{QStringLiteral("qinda-reclaimed")};
  const QStringList sessions{QStringLiteral("qindaqt.desktop")};
  const QStringList users{QStringLiteral("ada")};

  SddmOwnedChangeSet badTheme;
  badTheme.theme = QStringLiteral("no-such-theme");
  QVERIFY(validateOwnedChangeSet(badTheme, themes, sessions, users)
              .contains(QStringLiteral("no-such-theme")));

  SddmOwnedChangeSet badNumlock;
  badNumlock.numlock = QStringLiteral("maybe");
  QVERIFY(validateOwnedChangeSet(badNumlock, themes, sessions, users)
              .contains(QStringLiteral("maybe")));

  SddmOwnedChangeSet badSession;
  badSession.autologinSession = QStringLiteral("ghost.desktop");
  QVERIFY(validateOwnedChangeSet(badSession, themes, sessions, users)
              .contains(QStringLiteral("ghost.desktop")));

  SddmOwnedChangeSet badUser;
  badUser.autologinUser = QStringLiteral("mallory");
  QVERIFY(validateOwnedChangeSet(badUser, themes, sessions, users)
              .contains(QStringLiteral("mallory")));
}

void SddmOwnedConfigTest::validateAcceptsEmptyAutologinValues() {
  SddmOwnedChangeSet changes;
  changes.autologinUser = QString();    // autologin off
  changes.autologinSession = QString(); // no pinned session
  QCOMPARE(validateOwnedChangeSet(changes, {}, {}, {}), QString());
}

void SddmOwnedConfigTest::writeCreatesMergesAndKeepsPermissions() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString target = root.filePath(QStringLiteral("sddm.conf.d/zzz.conf"));
  QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));

  SddmOwnedChangeSet first;
  first.theme = QStringLiteral("qinda-reclaimed");
  QString error;
  QVERIFY2(writeOwnedChangeSetToFile(target, first, &error),
           qPrintable(error));
  QVERIFY(QFile::exists(target));
  // QFileDevice::Permissions is its own bitfield, not the unix mode, and Qt
  // reports the owner bits twice (Owner and User): unix 0644 reads back as
  // ReadOwner|WriteOwner|ReadUser|WriteUser|ReadGroup|ReadOther.
  QCOMPARE(QFile::permissions(target).toInt(),
           (QFileDevice::ReadOwner | QFileDevice::WriteOwner |
            QFileDevice::ReadUser | QFileDevice::WriteUser |
            QFileDevice::ReadGroup | QFileDevice::ReadOther)
               .toInt());

  SddmOwnedChangeSet second;
  second.numlock = QStringLiteral("on");
  QVERIFY2(writeOwnedChangeSetToFile(target, second, &error),
           qPrintable(error));
  QFile file(target);
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString content = QString::fromUtf8(file.readAll());
  QVERIFY(content.contains(QStringLiteral("Current=qinda-reclaimed")));
  QVERIFY(content.contains(QStringLiteral("Numlock=on")));
}

QTEST_MAIN(SddmOwnedConfigTest)
#include "tst_sddm_owned_config.moc"
