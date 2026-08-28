// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_test_support.h"

#include "qindaqt/shell_launcher/desktop_entry_parser.h"
#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QTest>

using namespace QindaQt::ShellLauncher;
using namespace QindaQt::ShellLauncher::TestSupport;

namespace {

DesktopEntryParseResult parseText(const QString &text)
{
  return DesktopEntryParser::parse(text);
}

} // namespace

class DesktopEntryParserTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void parsesWellFormedEntryWithAction();
  void keepsUnescapingAndSkipsLocaleVariants();
  void missingEntryGroupIsRejected();
  void duplicateEntryGroupIsRejected();
  void keyOutsideGroupIsRejected();
  void unsupportedTypeIsRejected();
  void missingNameIsRejected();
  void danglingEscapeIsRejected();
  void unknownEscapeIsRejected();
  void invalidKeyCharacterIsRejected();
  void emptyKeyNameIsRejected();
  void truncatedLocaleKeyIsRejected();
  void nonAsciiKeyNameIsRejected();
  void validLocaleShapesSkipPayload_data();
  void validLocaleShapesSkipPayload();
  void malformedLocaleShapesAreRejected_data();
  void malformedLocaleShapesAreRejected();
  void unknownActionReferenceIsRejected();
  void duplicateActionGroupIsRejected();
  void duplicateRecognizedKeyIsRejected();
  void whitespaceAroundEqualsAndEscapedListItemsAreAccepted();
  void invalidBooleanIsRejected();
  void actionIdsAreUniqueAndWellFormed();
  void oversizeDocumentIsRejected();
  void oversizeNameFieldIsRejected();
  void blankNameIsRejected();
  void hiddenFlagsAreReportedNotRejected();
  void unknownKeysAndGroupsAreNotDecoded();
};

void DesktopEntryParserTest::parsesWellFormedEntryWithAction()
{
  EntryTemplate entry;
  entry.name = QStringLiteral("Qinda Editor");
  entry.genericName = QStringLiteral("Text Editor");
  entry.comment = QStringLiteral("Write and edit documents");
  entry.categories = QStringLiteral("Office;WordProcessor;");
  entry.actionsValue = QStringLiteral("new-window;");
  entry.actionGroups = actionGroup(QStringLiteral("new-window"),
                                   QStringLiteral("New Window"),
                                   QStringLiteral("window-new"));

  const auto result = parseText(entry.toText());
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QVERIFY(!result.entry->hidden);
  QCOMPARE(result.entry->name, QStringLiteral("Qinda Editor"));
  QCOMPARE(result.entry->genericName, QStringLiteral("Text Editor"));
  QCOMPARE(result.entry->comment, QStringLiteral("Write and edit documents"));
  QCOMPARE(result.entry->iconName, QStringLiteral("applications-utilities"));
  QCOMPARE(result.entry->categories.size(), 2);
  QCOMPARE(result.entry->keywords.size(), 2);
  QCOMPARE(result.entry->actions.size(), 1);
  QCOMPARE(result.entry->actions.first().id, QStringLiteral("new-window"));
  QCOMPARE(result.entry->actions.first().name, QStringLiteral("New Window"));
  QCOMPARE(result.entry->actions.first().iconName, QStringLiteral("window-new"));
}

void DesktopEntryParserTest::keepsUnescapingAndSkipsLocaleVariants()
{
  const QString text = QStringLiteral(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=Space\\sApp\n"
      "Name[de]=Raum\\sApp\n"
      "Comment=Line one\\nLine two\n");

  const auto result = parseText(text);
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.entry->name, QStringLiteral("Space App"));
  QCOMPARE(result.entry->comment, QStringLiteral("Line one\nLine two"));
}

void DesktopEntryParserTest::missingEntryGroupIsRejected()
{
  const auto result =
      parseText(QStringLiteral("[X-Other]\nFuture=ignored\n"));
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::MissingEntryGroup);
}

void DesktopEntryParserTest::duplicateEntryGroupIsRejected()
{
  EntryTemplate entry;
  const QString text = entry.toText() + entry.toText();
  const auto result = parseText(text);
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::DuplicateEntryGroup);
  QCOMPARE(result.error.line, 7);
}

void DesktopEntryParserTest::keyOutsideGroupIsRejected()
{
  const auto result = parseText(QStringLiteral("Name=Orphan\n"));
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidKeyLine);
  QCOMPARE(result.error.line, 1);
}

void DesktopEntryParserTest::unsupportedTypeIsRejected()
{
  EntryTemplate entry;
  entry.type = QStringLiteral("Link");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::UnsupportedType);

  EntryTemplate missingType;
  missingType.type = QString();
  const auto missing = parseText(missingType.toText());
  QVERIFY(!missing.ok());
  QCOMPARE(missing.error.code, DesktopEntryErrorCode::UnsupportedType);
}

void DesktopEntryParserTest::missingNameIsRejected()
{
  EntryTemplate entry;
  entry.name = QString();
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::MissingName);
}

void DesktopEntryParserTest::danglingEscapeIsRejected()
{
  const auto result = parseText(
      QStringLiteral("[Desktop Entry]\nType=Application\nName=Broken\\\n"));
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidEscape);
  QCOMPARE(result.error.line, 3);
}

void DesktopEntryParserTest::unknownEscapeIsRejected()
{
  const auto result = parseText(
      QStringLiteral("[Desktop Entry]\nType=Application\nName=Bad\\xEscape\n"));
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidEscape);
}

void DesktopEntryParserTest::invalidKeyCharacterIsRejected()
{
  const auto result = parseText(QStringLiteral(
      "[Desktop Entry]\nType=Application\nNa me=Invalid Key\n"));
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidKeyLine);
}

void DesktopEntryParserTest::emptyKeyNameIsRejected()
{
  EntryTemplate entry;
  entry.extraBody = QStringLiteral("   =hostile");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidKeyLine);
}

void DesktopEntryParserTest::truncatedLocaleKeyIsRejected()
{
  EntryTemplate entry;
  entry.extraBody = QStringLiteral("Name[de=hostile\\x");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidKeyLine);
}

void DesktopEntryParserTest::nonAsciiKeyNameIsRejected()
{
  EntryTemplate entry;
  entry.extraBody = QStringLiteral("Nämé=hostile");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidKeyLine);
}

void DesktopEntryParserTest::validLocaleShapesSkipPayload_data()
{
  QTest::addColumn<QString>("key");

  QTest::newRow("language") << QStringLiteral("Name[en]");
  QTest::newRow("country") << QStringLiteral("Name[en_US]");
  QTest::newRow("encoding") << QStringLiteral("Name[en.UTF-8]");
  QTest::newRow("modifier") << QStringLiteral("Name[en@latin]");
  QTest::newRow("full") << QStringLiteral("Name[en_US.UTF-8@latin]");
}

void DesktopEntryParserTest::validLocaleShapesSkipPayload()
{
  QFETCH(QString, key);
  EntryTemplate entry;
  entry.extraBody = key + QStringLiteral("=bad\\x");
  const auto result = parseText(entry.toText());
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.entry->name, QStringLiteral("Fixture App"));
}

void DesktopEntryParserTest::malformedLocaleShapesAreRejected_data()
{
  QTest::addColumn<QString>("key");

  QTest::newRow("missing-language") << QStringLiteral("Name[@]");
  QTest::newRow("missing-language-country") << QStringLiteral("Name[_US]");
  QTest::newRow("empty-country") << QStringLiteral("Name[en_]");
  QTest::newRow("empty-encoding") << QStringLiteral("Name[en.]");
  QTest::newRow("empty-modifier") << QStringLiteral("Name[en@]");
  QTest::newRow("repeated-country") << QStringLiteral("Name[en_US_GB]");
  QTest::newRow("repeated-encoding") << QStringLiteral("Name[en.UTF.8]");
  QTest::newRow("repeated-modifier") << QStringLiteral("Name[en@latin@formal]");
  QTest::newRow("country-after-encoding") << QStringLiteral("Name[en.UTF-8_US]");
  QTest::newRow("country-after-modifier") << QStringLiteral("Name[en@latin_US]");
  QTest::newRow("encoding-after-modifier") << QStringLiteral("Name[en@latin.UTF-8]");
}

void DesktopEntryParserTest::malformedLocaleShapesAreRejected()
{
  QFETCH(QString, key);
  EntryTemplate entry;
  entry.extraBody = key + QStringLiteral("=bad\\x");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidKeyLine);
}

void DesktopEntryParserTest::unknownActionReferenceIsRejected()
{
  EntryTemplate entry;
  entry.actionsValue = QStringLiteral("missing;");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::UnknownActionReference);

  EntryTemplate unnamed;
  unnamed.actionsValue = QStringLiteral("blank;");
  unnamed.actionGroups = actionGroup(QStringLiteral("blank"), QString());
  const auto unnamedResult = parseText(unnamed.toText());
  QVERIFY(!unnamedResult.ok());
  QCOMPARE(unnamedResult.error.code, DesktopEntryErrorCode::UnknownActionReference);
}

void DesktopEntryParserTest::duplicateActionGroupIsRejected()
{
  EntryTemplate entry;
  entry.actionsValue = QStringLiteral("new-window;");
  entry.actionGroups = actionGroup(QStringLiteral("new-window"),
                                   QStringLiteral("First"),
                                   QStringLiteral("first-icon"))
                       + actionGroup(QStringLiteral("new-window"),
                                     QStringLiteral("Second"),
                                     QStringLiteral("second-icon"));
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::DuplicateActionGroup);
}

void DesktopEntryParserTest::duplicateRecognizedKeyIsRejected()
{
  EntryTemplate entry;
  entry.extraBody = QStringLiteral("Name=Second");
  const auto entryResult = parseText(entry.toText());
  QVERIFY(!entryResult.ok());
  QCOMPARE(entryResult.error.code, DesktopEntryErrorCode::DuplicateKey);

  entry.extraBody.clear();
  entry.actionsValue = QStringLiteral("new-window;");
  entry.actionGroups = QStringLiteral(
      "\n[Desktop Action new-window]\nName=First\nName=Second\n");
  const auto actionResult = parseText(entry.toText());
  QVERIFY(!actionResult.ok());
  QCOMPARE(actionResult.error.code, DesktopEntryErrorCode::DuplicateKey);
}

void DesktopEntryParserTest::whitespaceAroundEqualsAndEscapedListItemsAreAccepted()
{
  const auto result = parseText(QStringLiteral(
      "[Desktop Entry]\n"
      "Type = Application\n"
      "Name = Spaced App\n"
      "Categories = Utility\\;Tools;Office;\n"
      "Keywords = one\\;two;three;\n"));
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.entry->name, QStringLiteral("Spaced App"));
  QCOMPARE(result.entry->categories,
           QStringList({ QStringLiteral("Utility;Tools"),
                         QStringLiteral("Office") }));
  QCOMPARE(result.entry->keywords,
           QStringList({ QStringLiteral("one;two"), QStringLiteral("three") }));
}

void DesktopEntryParserTest::invalidBooleanIsRejected()
{
  EntryTemplate entry;
  entry.hiddenLine = QStringLiteral("Hidden=yes");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::InvalidBoolean);
}

void DesktopEntryParserTest::actionIdsAreUniqueAndWellFormed()
{
  EntryTemplate duplicate;
  duplicate.actionsValue = QStringLiteral("open;open;");
  duplicate.actionGroups = actionGroup(QStringLiteral("open"),
                                       QStringLiteral("Open"));
  const auto duplicateResult = parseText(duplicate.toText());
  QVERIFY(!duplicateResult.ok());
  QCOMPARE(duplicateResult.error.code, DesktopEntryErrorCode::InvalidActionId);

  EntryTemplate malformed;
  malformed.actionsValue = QStringLiteral("not_valid;");
  malformed.actionGroups = actionGroup(QStringLiteral("not_valid"),
                                       QStringLiteral("Open"));
  const auto malformedResult = parseText(malformed.toText());
  QVERIFY(!malformedResult.ok());
  QCOMPARE(malformedResult.error.code, DesktopEntryErrorCode::InvalidActionId);

  EntryTemplate empty;
  empty.actionsValue = QStringLiteral(";");
  const auto emptyResult = parseText(empty.toText());
  QVERIFY(!emptyResult.ok());
  QCOMPARE(emptyResult.error.code, DesktopEntryErrorCode::InvalidActionId);
}

void DesktopEntryParserTest::oversizeDocumentIsRejected()
{
  EntryTemplate entry;
  entry.extraBody = QStringLiteral("Pad=%1").arg(
      QString(Bounds::maxDocumentCodeUnits + 8, QLatin1Char('x')));
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::DocumentTooLarge);
}

void DesktopEntryParserTest::blankNameIsRejected()
{
  EntryTemplate entry;
  entry.name = QStringLiteral("   ");
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::MissingName);
}

void DesktopEntryParserTest::oversizeNameFieldIsRejected()
{
  EntryTemplate entry;
  entry.name = QString(Bounds::maxNameLength + 1, QLatin1Char('N'));
  const auto result = parseText(entry.toText());
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, DesktopEntryErrorCode::FieldLimitExceeded);
}

void DesktopEntryParserTest::hiddenFlagsAreReportedNotRejected()
{
  EntryTemplate noDisplay;
  noDisplay.noDisplayLine = QStringLiteral("NoDisplay=true");
  const auto noDisplayResult = parseText(noDisplay.toText());
  QVERIFY2(noDisplayResult.ok(), qPrintable(noDisplayResult.error.message));
  QVERIFY(noDisplayResult.entry->hidden);

  EntryTemplate hidden;
  hidden.hiddenLine = QStringLiteral("Hidden=true");
  const auto hiddenResult = parseText(hidden.toText());
  QVERIFY2(hiddenResult.ok(), qPrintable(hiddenResult.error.message));
  QVERIFY(hiddenResult.entry->hidden);

  EntryTemplate visible;
  visible.noDisplayLine = QStringLiteral("NoDisplay=false");
  const auto visibleResult = parseText(visible.toText());
  QVERIFY2(visibleResult.ok(), qPrintable(visibleResult.error.message));
  QVERIFY(!visibleResult.entry->hidden);
}

void DesktopEntryParserTest::unknownKeysAndGroupsAreNotDecoded()
{
  EntryTemplate entry;
  entry.extraBody = QStringLiteral(
      "Exec=qinda-editor %U\n"
      "X-Test=bad\\xescape\n"
      "[X-Future Group]\n"
      "Future=also\\qignored");
  const auto result = parseText(entry.toText());
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.entry->name, QStringLiteral("Fixture App"));
}

QTEST_GUILESS_MAIN(DesktopEntryParserTest)
#include "tst_desktop_entry_parser.moc"
